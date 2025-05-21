#include "udp_gateway/gateway.hpp"
#include "udp_gateway/rate_limiter.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <functional>

// Mock interface declarations for compilation
// In a real implementation, these would be included from their respective components
namespace protocol_parser {
    class IProtocolParser {
    public:
        virtual ~IProtocolParser() = default;
        virtual udp_gateway::Result<udp_gateway::DeviceMessage> parseMessage(
            const std::vector<uint8_t>& data) = 0;
        virtual std::vector<uint8_t> createAcknowledgment(
            const udp_gateway::MessageAcknowledgment& ack) = 0;
        virtual std::vector<uint8_t> createCommandPacket(
            const std::vector<udp_gateway::DeviceCommand>& commands) = 0;
    };
}

namespace redis_deduplication {
    class IDeduplicationService {
    public:
        virtual ~IDeduplicationService() = default;
        virtual bool isDuplicate(
            const std::string& deviceId,
            uint32_t sequenceNumber,
            const std::string& messageId) = 0;
        virtual void recordMessage(
            const std::string& deviceId,
            uint32_t sequenceNumber,
            const std::string& messageId) = 0;
    };
}

namespace rabbitmq_integration {
    class IMessagePublisher {
    public:
        virtual ~IMessagePublisher() = default;
        virtual bool publishMessage(
            const std::string& queueName,
            const std::vector<uint8_t>& data) = 0;
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

Gateway::Gateway(const Config& config)
    : config_(config)
    , socket_(ioContext_)
    , rateLimiter_(createDefaultRateLimiter())
{
    // Initialize with default components - in real implementation these would be from
    // the component factories in their respective libraries
    initialize();
}

Gateway::Gateway(
    const Config& config,
    std::shared_ptr<protocol_parser::IProtocolParser> protocolParser,
    std::shared_ptr<redis_deduplication::IDeduplicationService> deduplicationService,
    std::shared_ptr<rabbitmq_integration::IMessagePublisher> messagePublisher,
    std::shared_ptr<device_manager::IDeviceManager> deviceManager,
    std::shared_ptr<IRateLimiter> rateLimiter,
    std::shared_ptr<monitoring_system::IMetricsCollector> metricsCollector
)
    : config_(config)
    , socket_(ioContext_)
    , protocolParser_(std::move(protocolParser))
    , deduplicationService_(std::move(deduplicationService))
    , messagePublisher_(std::move(messagePublisher))
    , deviceManager_(std::move(deviceManager))
    , rateLimiter_(std::move(rateLimiter))
    , metricsCollector_(std::move(metricsCollector))
{
}

Gateway::~Gateway() {
    stop();
}

void Gateway::initialize() {
    // Set the number of worker threads if not specified
    if (config_.numWorkerThreads == 0) {
        config_.numWorkerThreads = std::thread::hardware_concurrency();
        if (config_.numWorkerThreads == 0) {
            config_.numWorkerThreads = 4;  // Reasonable default if hardware_concurrency() fails
        }
    }
    
    // Initialize rate limiter configuration if we created it ourselves
    if (dynamic_cast<TokenBucketRateLimiter*>(rateLimiter_.get())) {
        // In a real implementation, we might adjust the rate limiter config here
        // or create a new limiter with the desired settings
        // For now, we'll just assume the config provided is acceptable
    }
    
    // In a real implementation, we would initialize all the components here
    if (!protocolParser_) {
        // protocolParser_ = protocol_parser::createParser();
    }
    
    if (!deduplicationService_) {
        // deduplicationService_ = redis_deduplication::createService();
    }
    
    if (!messagePublisher_) {
        // messagePublisher_ = rabbitmq_integration::createPublisher();
    }
    
    if (!deviceManager_) {
        // deviceManager_ = device_manager::createManager();
    }
    
    if (!metricsCollector_ && config_.enableMetrics) {
        // metricsCollector_ = monitoring_system::createCollector();
    }
}

VoidResult Gateway::start() {
    if (running_) {
        return makeErrorResult(
            ErrorCode::INVALID_CONFIGURATION,
            "Gateway is already running"
        );
    }
    
    try {
        // Set up the socket
        setupSocket();
        
        // Create an io_context work guard to keep run() from returning
        workGuard_ = std::make_unique<boost::asio::io_context::work>(ioContext_);
        
        // Start worker threads
        for (size_t i = 0; i < config_.numWorkerThreads; ++i) {
            workerThreads_.emplace_back([this] { this->workerThread(); });
        }
        
        // Set up metrics timer if enabled
        if (config_.enableMetrics && metricsCollector_) {
            metricsTimer_ = std::make_unique<boost::asio::steady_timer>(
                ioContext_, std::chrono::seconds(config_.metricsInterval)
            );
            updateMetrics();
        }
        
        // Start accepting connections
        startAccept();
        
        // Mark as running
        running_ = true;
        
        return makeSuccessResult();
    } catch (const std::exception& e) {
        return makeErrorResult(
            ErrorCode::INTERNAL_ERROR,
            std::string("Failed to start gateway: ") + e.what()
        );
    }
}

void Gateway::stop() {
    if (!running_) {
        return;
    }
    
    // Mark as not running
    running_ = false;
    
    // Cancel the work guard to allow the io_context to exit
    workGuard_.reset();
    
    // Cancel the metrics timer
    if (metricsTimer_) {
        boost::system::error_code ec;
        metricsTimer_->cancel(ec);
    }
    
    // Cancel any pending socket operations
    boost::system::error_code ec;
    socket_.cancel(ec);
    
    try {
        // Stop all sessions
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            for (auto& session : activeSessions_) {
                session.second->stop();
            }
            activeSessions_.clear();
        }
        
        // Stop the io_context
        ioContext_.stop();
        
        // Wait for all worker threads to finish
        for (auto& thread : workerThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        workerThreads_.clear();
    } catch (const std::exception& e) {
        std::cerr << "Error during gateway shutdown: " << e.what() << std::endl;
    }
}

bool Gateway::isRunning() const {
    return running_;
}

GatewayMetrics Gateway::getMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

size_t Gateway::getActiveConnectionCount() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return activeSessions_.size();
}

void Gateway::setupSocket() {
    // Create the appropriate endpoint for IPv4 or IPv6
    boost::asio::ip::udp::endpoint endpoint;
    
    if (config_.enableIPv6) {
        endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::address_v6::from_string(config_.listenAddress),
            config_.listenPort
        );
    } else {
        endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::address_v4::from_string(config_.listenAddress),
            config_.listenPort
        );
    }
    
    // Open the socket
    socket_.open(endpoint.protocol());
    
    // Set socket options
    socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    
    // Bind to the endpoint
    socket_.bind(endpoint);
}

void Gateway::startAccept() {
    // This is more of a placeholder since UDP is connectionless
    // In a real implementation, we would set up initial receiver(s)
    // For now, we'll create a single session for the main socket
    
    if (!running_) {
        return;
    }
    
    // Create a session configuration
    Session::Config sessionConfig;
    sessionConfig.regionCode = config_.regionCode;
    sessionConfig.maxMessageSize = config_.maxMessageSize;
    sessionConfig.enableDebugging = config_.enableDebugging;
    sessionConfig.enableAcknowledgments = config_.enableAcknowledgments;
    sessionConfig.processingTimeout = config_.processingTimeout;
    
    try {
        // Create a session
        auto session = std::make_shared<Session>(
            ioContext_,
            boost::asio::ip::udp::socket(ioContext_, socket_.local_endpoint().protocol()),
            sessionConfig,
            protocolParser_,
            deduplicationService_,
            messagePublisher_,
            deviceManager_,
            rateLimiter_,
            metricsCollector_,
            this->shared_from_this()  // ISessionHandler - using this-> for clarity
        );
        
        // Start the session
        session->start();
        
        // Store the session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            activeSessions_[socket_.local_endpoint()] = std::move(session);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating initial session: " << e.what() << std::endl;
    }
}

void Gateway::handleAccept(const boost::system::error_code& error, 
                          const boost::asio::ip::udp::endpoint& endpoint) {
    if (!running_) {
        return;
    }
    
    if (!error) {
        // We received a new connection, handle it
        // For UDP, this would typically be a datagram from a new endpoint
        
        // In a real implementation, we would create a new session or
        // handle this differently since UDP is connectionless
        std::cout << "Received connection from: " << endpoint.address().to_string() 
                  << ":" << endpoint.port() << std::endl;
    } else {
        std::cerr << "Accept error: " << error.message() << std::endl;
    }
    
    // Continue accepting
    startAccept();
}

void Gateway::workerThread() {
    try {
        // Run the io_context
        ioContext_.run();
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
    }
}

void Gateway::updateMetrics() {
    if (!config_.enableMetrics || !metricsCollector_ || !running_) {
        return;
    }
    
    try {
        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            
            // Update last updated timestamp
            metrics_.lastUpdated = std::chrono::system_clock::now();
            
            // Update active connections
            metrics_.activeConnections = getActiveConnectionCount();
            
            // Update peak connections if needed
            metrics_.peakConnections = std::max(metrics_.peakConnections, metrics_.activeConnections);
            
            // Other metrics would be updated by the components
        }
        
        // Record metrics with the metrics collector
        metricsCollector_->recordGauge("active_connections", metrics_.activeConnections);
        metricsCollector_->recordGauge("peak_connections", metrics_.peakConnections);
        
        // Schedule the next update
        metricsTimer_->expires_after(std::chrono::seconds(config_.metricsInterval));
        metricsTimer_->async_wait([this](const boost::system::error_code& error) {
            if (!error) {
                this->updateMetrics();
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "Error updating metrics: " << e.what() << std::endl;
    }
}

std::string Gateway::createQueueName(const std::string& imei, QueueType queueType) const {
    std::ostringstream oss;
    
    // Add optional prefix
    if (!config_.queuePrefix.empty()) {
        oss << config_.queuePrefix << ".";
    }
    
    // Add region code
    oss << config_.regionCode << ".";
    
    // Add IMEI
    oss << imei << "_";
    
    // Add queue type suffix
    switch (queueType) {
        case QueueType::INBOUND:
            oss << "inbound";
            break;
        case QueueType::DEBUG:
            oss << "debug";
            break;
        case QueueType::DISCARD:
            oss << "discard";
            break;
        case QueueType::OUTBOUND:
            oss << "outbound";
            break;
    }
    
    return oss.str();
}

// Implementation of ISessionHandler interface
VoidResult Gateway::handleMessage(const DeviceMessage& message) {
    try {
        // Create the appropriate queue name based on message type
        std::string queueName = createQueueName(message.deviceId, QueueType::INBOUND);
        
        // Publish the message to the queue
        bool published = messagePublisher_->publishMessage(queueName, message.payload);
        
        if (!published) {
            return makeErrorResult(
                ErrorCode::DEPENDENCY_FAILURE,
                "Failed to publish message to queue: " + queueName
            );
        }
        
        // If debugging is enabled, also publish to the debug queue
        if (config_.enableDebugging) {
            std::string debugQueueName = createQueueName(message.deviceId, QueueType::DEBUG);
            messagePublisher_->publishMessage(debugQueueName, message.payload);
            // Not checking result, debug queue is not critical
        }
        
        // Update metrics
        if (metricsCollector_) {
            metricsCollector_->incrementCounter("messages_published");
            
            // Update region-specific metrics
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                metrics_.totalPacketsProcessed++;
                metrics_.packetsPerRegion[message.region]++;
            }
        }
        
        return makeSuccessResult();
    } catch (const std::exception& e) {
        return makeErrorResult(
            ErrorCode::INTERNAL_ERROR,
            std::string("Error handling message: ") + e.what()
        );
    }
}

void Gateway::handleError(const IMEI& deviceId, ErrorCode errorCode, const std::string& errorMessage) {
    try {
        // Log the error
        std::cerr << "Error handling message from device " << deviceId 
                  << ": [" << errorCodeToString(errorCode) << "] " 
                  << errorMessage << std::endl;
        
        // For certain types of errors, we might want to publish to the discard queue
        if (errorCode == ErrorCode::INVALID_MESSAGE ||
            errorCode == ErrorCode::AUTHENTICATION_FAILURE) {
            
            // Create a payload with error information
            // In a real implementation, we would use a proper protocol
            std::string errorPayload = "Error: " + errorCodeToString(errorCode) + " - " + errorMessage;
            std::vector<uint8_t> payload(errorPayload.begin(), errorPayload.end());
            
            // Publish to the discard queue
            std::string discardQueueName = createQueueName(deviceId, QueueType::DISCARD);
            messagePublisher_->publishMessage(discardQueueName, payload);
        }
        
        // Update metrics
        if (metricsCollector_) {
            metricsCollector_->recordError(errorCodeToString(errorCode));
            
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                metrics_.totalPacketsRejected++;
                metrics_.errorsPerType[errorCodeToString(errorCode)]++;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in handleError: " << e.what() << std::endl;
    }
}

std::vector<DeviceCommand> Gateway::getPendingCommands(const IMEI& deviceId) {
    try {
        // Get pending commands from the device manager
        return deviceManager_->getPendingCommands(deviceId);
    } catch (const std::exception& e) {
        std::cerr << "Error getting pending commands: " << e.what() << std::endl;
        return {};  // Return empty vector on error
    }
}

} // namespace udp_gateway