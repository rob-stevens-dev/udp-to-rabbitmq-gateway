#include "udp_gateway/session.hpp"
#include "udp_gateway/rate_limiter.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

// Include interfaces from other components
// In a real implementation, these would be from their respective components
namespace protocol_parser {
    class IProtocolParser {
    public:
        virtual ~IProtocolParser() = default;
        virtual udp_gateway::Result<udp_gateway::DeviceMessage> parseMessage(const std::vector<uint8_t>& data) = 0;
        virtual std::vector<uint8_t> createAcknowledgment(const udp_gateway::MessageAcknowledgment& ack) = 0;
        virtual std::vector<uint8_t> createCommandPacket(const std::vector<udp_gateway::DeviceCommand>& commands) = 0;
    };
}

namespace redis_deduplication {
    class IDeduplicationService {
    public:
        virtual ~IDeduplicationService() = default;
        virtual bool isDuplicate(const udp_gateway::IMEI& deviceId, udp_gateway::SequenceNumber sequenceNumber, const std::string& messageId) = 0;
        virtual void recordMessage(const udp_gateway::IMEI& deviceId, udp_gateway::SequenceNumber sequenceNumber, const std::string& messageId) = 0;
    };
}

namespace rabbitmq_integration {
    class IMessagePublisher {
    public:
        virtual ~IMessagePublisher() = default;
        virtual bool publishMessage(const std::string& queueName, const std::vector<uint8_t>& data) = 0;
    };
}

namespace device_manager {
    class IDeviceManager {
    public:
        virtual ~IDeviceManager() = default;
        virtual bool isAuthenticated(const udp_gateway::IMEI& deviceId) = 0;
        virtual udp_gateway::DeviceState getDeviceState(const udp_gateway::IMEI& deviceId) = 0;
        virtual void updateDeviceStatus(const udp_gateway::IMEI& deviceId, udp_gateway::DeviceState state) = 0;
        virtual std::vector<udp_gateway::DeviceCommand> getPendingCommands(const udp_gateway::IMEI& deviceId) = 0;
        virtual void markCommandDelivered(const std::string& commandId) = 0;
    };
}

namespace monitoring_system {
    class IMetricsCollector {
    public:
        virtual ~IMetricsCollector() = default;
        virtual void incrementCounter(const std::string& name, uint64_t value = 1) = 0;
        virtual void recordGauge(const std::string& name, double value) = 0;
        virtual void recordHistogram(const std::string& name, double value) = 0;
        virtual void recordError(const std::string& errorType) = 0;
    };
}

namespace udp_gateway {

Session::Session(
    boost::asio::io_context& ioContext,
    boost::asio::ip::udp::socket socket,
    const Config& config,
    std::shared_ptr<protocol_parser::IProtocolParser> protocolParser,
    std::shared_ptr<redis_deduplication::IDeduplicationService> deduplicationService,
    std::shared_ptr<rabbitmq_integration::IMessagePublisher> messagePublisher,
    std::shared_ptr<device_manager::IDeviceManager> deviceManager,
    std::shared_ptr<IRateLimiter> rateLimiter,
    std::shared_ptr<monitoring_system::IMetricsCollector> metricsCollector,
    std::shared_ptr<ISessionHandler> sessionHandler
)
    : ioContext_(ioContext)
    , socket_(std::move(socket))
    , config_(config)
    , receiveBuffer_(config.maxMessageSize)
    , protocolParser_(std::move(protocolParser))
    , deduplicationService_(std::move(deduplicationService))
    , messagePublisher_(std::move(messagePublisher))
    , deviceManager_(std::move(deviceManager))
    , rateLimiter_(std::move(rateLimiter))
    , metricsCollector_(std::move(metricsCollector))
    , sessionHandler_(std::move(sessionHandler))
{
}

void Session::start() {
    active_ = true;
    doReceive();
}

void Session::stop() {
    active_ = false;
    
    // Cancel any pending operations
    boost::system::error_code ec;
    socket_.cancel(ec);
    
    // Don't worry about the error code, we're stopping anyway
}

bool Session::isActive() const {
    return active_;
}

void Session::doReceive() {
    if (!active_) {
        return;
    }
    
    // Set up a receive operation
    socket_.async_receive_from(
        boost::asio::buffer(receiveBuffer_),
        senderEndpoint_,
        [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytesReceived) {
            self->handlePacket(error, bytesReceived);
        }
    );
}

void Session::handlePacket(const boost::system::error_code& error, std::size_t bytesReceived) {
    if (!active_) {
        return;
    }
    
    if (!error && bytesReceived > 0) {
        // We received data, process it
        packetsReceived_++;
        bytesReceived_ += bytesReceived;
        
        // Create a copy of the received data for processing
        std::vector<uint8_t> packetData(receiveBuffer_.begin(), receiveBuffer_.begin() + bytesReceived);
        
        try {
            // Process the packet (potentially time-consuming operation)
            processPacket(packetData, senderEndpoint_);
            packetsProcessed_++;
        } catch (const std::exception& e) {
            // Log the error
            std::cerr << "Error processing packet: " << e.what() << std::endl;
            packetsRejected_++;
            
            // Record error in metrics
            if (metricsCollector_) {
                metricsCollector_->recordError("packet_processing_exception");
            }
        }
    } else if (error) {
        // Handle error
        if (error != boost::asio::error::operation_aborted) {
            std::cerr << "Receive error: " << error.message() << std::endl;
            
            // Record error in metrics
            if (metricsCollector_) {
                metricsCollector_->recordError("socket_error");
            }
        }
    }
    
    // Continue receiving
    doReceive();
}

void Session::processPacket(const std::vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& sender) {
    // Parse and validate the packet
    auto result = parseAndValidate(data, sender);
    
    if (result) {
        // Packet is valid, handle the message
        const auto& message = result.value;
        
        // Update metrics
        updateMetrics(true);
        
        // Send the message to the session handler
        auto handleResult = sessionHandler_->handleMessage(message);
        
        if (handleResult) {
            // Send acknowledgment if required
            if (message.requiresAcknowledgment && config_.enableAcknowledgments) {
                sendAcknowledgment(message, ErrorCode::SUCCESS);
            }
            
            // Check for and send any pending commands
            sendCommands(message.deviceId, sender);
        } else {
            // Session handler failed to process the message
            if (message.requiresAcknowledgment && config_.enableAcknowledgments) {
                sendAcknowledgment(message, handleResult.errorCode);
            }
            
            // Log the error
            logProcessingError(message.deviceId, handleResult.errorCode, handleResult.errorMessage);
        }
    } else {
        // Packet is invalid
        IMEI deviceId = "unknown";  // Default device ID
        
        // Try to extract device ID from the data for better error reporting
        try {
            // This is a simplified approach; real implementation would use the protocol parser
            // to extract the device ID even if the message is otherwise invalid
            if (data.size() >= 15) {  // Simplified example: IMEI is the first 15 bytes
                deviceId = std::string(data.begin(), data.begin() + 15);
            }
        } catch (...) {
            // Ignore extraction errors
        }
        
        // Log the error
        logProcessingError(deviceId, result.errorCode, result.errorMessage);
        
        // Update metrics
        updateMetrics(false, errorCodeToString(result.errorCode));
        
        // Notify the session handler about the error
        sessionHandler_->handleError(deviceId, result.errorCode, result.errorMessage);
    }
}

Result<DeviceMessage> Session::parseAndValidate(const std::vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& sender) {
    // Mark sender as unused to avoid compiler warnings
    (void)sender;  // Unused parameter
    
    // Step 1: Parse the message using the protocol parser
    auto parseResult = protocolParser_->parseMessage(data);
    if (!parseResult) {
        return Result<DeviceMessage>::error(
            ErrorCode::INVALID_MESSAGE,
            "Failed to parse message: " + parseResult.errorMessage
        );
    }
    
    auto& message = parseResult.value;
    
    // Add region code to the message
    message.region = config_.regionCode;
    
    // Step 2: Authenticate the device
    if (!deviceManager_->isAuthenticated(message.deviceId)) {
        return Result<DeviceMessage>::error(
            ErrorCode::AUTHENTICATION_FAILURE,
            "Device is not authenticated: " + message.deviceId
        );
    }
    
    // Step 3: Check device state
    auto deviceState = deviceManager_->getDeviceState(message.deviceId);
    if (deviceState == DeviceState::SUSPENDED) {
        return Result<DeviceMessage>::error(
            ErrorCode::DEVICE_SUSPENDED,
            "Device is suspended: " + message.deviceId
        );
    }
    
    // Step 4: Check for duplicate message
    if (deduplicationService_->isDuplicate(message.deviceId, message.sequenceNumber, message.messageId)) {
        return Result<DeviceMessage>::error(
            ErrorCode::DUPLICATE_MESSAGE,
            "Duplicate message detected: " + message.messageId
        );
    }
    
    // Step 5: Check rate limit
    if (!rateLimiter_->checkLimit(message.deviceId, message.priority)) {
        return Result<DeviceMessage>::error(
            ErrorCode::RATE_LIMITED,
            "Device exceeded rate limit: " + message.deviceId
        );
    }
    
    // Message is valid, record it for deduplication
    deduplicationService_->recordMessage(message.deviceId, message.sequenceNumber, message.messageId);
    
    // Record the message for rate limiting
    rateLimiter_->recordMessage(message.deviceId, message.priority, data.size());
    
    // Update device status to connected
    deviceManager_->updateDeviceStatus(message.deviceId, DeviceState::CONNECTED);
    
    return Result<DeviceMessage>::ok(message);
}

void Session::sendAcknowledgment(const DeviceMessage& message, ErrorCode errorCode) {
    // Create acknowledgment struct
    MessageAcknowledgment ack;
    ack.deviceId = message.deviceId;
    ack.messageId = message.messageId;
    ack.success = (errorCode == ErrorCode::SUCCESS);
    
    if (!ack.success) {
        ack.errorReason = errorCodeToString(errorCode);
    }
    
    // Create acknowledgment packet
    auto ackPacket = protocolParser_->createAcknowledgment(ack);
    
    // Send the acknowledgment
    socket_.async_send_to(
        boost::asio::buffer(ackPacket),
        senderEndpoint_,
        [](const boost::system::error_code& error, std::size_t /*bytesSent*/) {
            if (error) {
                std::cerr << "Error sending acknowledgment: " << error.message() << std::endl;
            }
        }
    );
    
    // Update metrics
    if (metricsCollector_) {
        metricsCollector_->incrementCounter("acknowledgments_sent");
    }
}

void Session::sendCommands(const IMEI& deviceId, const boost::asio::ip::udp::endpoint& endpoint) {
    // Get pending commands for this device
    auto commands = sessionHandler_->getPendingCommands(deviceId);
    
    if (commands.empty()) {
        return;  // No commands to send
    }
    
    // Create command packet
    auto commandPacket = protocolParser_->createCommandPacket(commands);
    
    // Send the commands
    socket_.async_send_to(
        boost::asio::buffer(commandPacket),
        endpoint,
        [](const boost::system::error_code& error, std::size_t /*bytesSent*/) {
            if (error) {
                std::cerr << "Error sending commands: " << error.message() << std::endl;
            }
        }
    );
    
    // Mark commands as delivered
    for (const auto& command : commands) {
        deviceManager_->markCommandDelivered(command.commandId);
    }
    
    // Update metrics
    if (metricsCollector_) {
        metricsCollector_->incrementCounter("commands_sent", commands.size());
    }
}

void Session::logProcessingError(const IMEI& deviceId, ErrorCode errorCode, const std::string& message) {
    std::ostringstream oss;
    oss << "Error processing message from device " << deviceId 
        << ": [" << errorCodeToString(errorCode) << "] " << message;
    std::cerr << oss.str() << std::endl;
}

void Session::updateMetrics(bool success, const std::string& errorType) {
    if (!metricsCollector_) {
        return;
    }
    
    if (success) {
        metricsCollector_->incrementCounter("packets_processed");
    } else {
        metricsCollector_->incrementCounter("packets_rejected");
        if (!errorType.empty()) {
            metricsCollector_->recordError(errorType);
        }
    }
}

} // namespace udp_gateway