#pragma once

#include "udp_gateway/types.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Forward declarations for component interfaces
namespace protocol_parser { class IProtocolParser; }
namespace redis_deduplication { class IDeduplicationService; }
namespace rabbitmq_integration { class IMessagePublisher; }
namespace device_manager { class IDeviceManager; }
namespace monitoring_system { class IMetricsCollector; }

namespace udp_gateway {

// Forward declaration
class IRateLimiter;

/**
 * @class ISessionHandler
 * @brief Interface for handling processed device messages
 * 
 * This interface defines the contract for components that will process
 * messages after they've been received and validated by a session.
 */
class ISessionHandler {
public:
    virtual ~ISessionHandler() = default;

    /**
     * @brief Handle a valid device message
     * 
     * @param message The validated device message
     * @return Result indicating success or failure
     */
    virtual VoidResult handleMessage(const DeviceMessage& message) = 0;

    /**
     * @brief Handle an error during message processing
     * 
     * @param deviceId The device identifier (IMEI)
     * @param errorCode The error code
     * @param errorMessage Detailed error message
     */
    virtual void handleError(const IMEI& deviceId, ErrorCode errorCode, const std::string& errorMessage) = 0;

    /**
     * @brief Get pending commands for a device
     * 
     * @param deviceId The device identifier (IMEI)
     * @return Vector of commands pending for the device
     */
    virtual std::vector<DeviceCommand> getPendingCommands(const IMEI& deviceId) = 0;
};

/**
 * @class Session
 * @brief Handles individual UDP message processing sessions
 * 
 * This class is responsible for processing individual UDP messages,
 * validating them, and passing them to the appropriate handlers.
 */
class Session : public std::enable_shared_from_this<Session> {
public:
    /**
     * @brief Structure for session configuration
     */
    struct Config {
        std::string regionCode;
        size_t maxMessageSize = 8192;
        bool enableDebugging = false;
        bool enableAcknowledgments = true;
        std::chrono::milliseconds processingTimeout{500}; // 500ms default timeout
    };

    /**
     * @brief Construct a new Session object
     * 
     * @param ioContext Boost.Asio IO context
     * @param socket UDP socket for this session
     * @param config Session configuration
     * @param protocolParser Protocol parser component
     * @param deduplicationService Deduplication service component
     * @param messagePublisher Message publisher component
     * @param deviceManager Device manager component
     * @param rateLimiter Rate limiter component
     * @param metricsCollector Metrics collector component
     * @param sessionHandler Session handler for processed messages
     */
    Session(
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
    );

    /**
     * @brief Start processing incoming UDP packets
     */
    void start();

    /**
     * @brief Stop the session
     */
    void stop();

    /**
     * @brief Check if the session is currently active
     * 
     * @return true if the session is active, false otherwise
     */
    bool isActive() const;

private:
    void doReceive();
    void handlePacket(const boost::system::error_code& error, std::size_t bytesReceived);
    void processPacket(const std::vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& sender);
    Result<DeviceMessage> parseAndValidate(const std::vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& sender);
    void sendAcknowledgment(const DeviceMessage& message, ErrorCode errorCode);
    void sendCommands(const IMEI& deviceId, const boost::asio::ip::udp::endpoint& endpoint);
    void logProcessingError(const IMEI& deviceId, ErrorCode errorCode, const std::string& message);
    void updateMetrics(bool success, const std::string& errorType = "");

    // Core members
    boost::asio::io_context& ioContext_;
    boost::asio::ip::udp::socket socket_;
    Config config_;
    std::vector<uint8_t> receiveBuffer_;
    boost::asio::ip::udp::endpoint senderEndpoint_;
    bool active_ = false;

    // Component dependencies
    std::shared_ptr<protocol_parser::IProtocolParser> protocolParser_;
    std::shared_ptr<redis_deduplication::IDeduplicationService> deduplicationService_;
    std::shared_ptr<rabbitmq_integration::IMessagePublisher> messagePublisher_;
    std::shared_ptr<device_manager::IDeviceManager> deviceManager_;
    std::shared_ptr<IRateLimiter> rateLimiter_;
    std::shared_ptr<monitoring_system::IMetricsCollector> metricsCollector_;
    std::shared_ptr<ISessionHandler> sessionHandler_;

    // Session statistics
    uint64_t packetsReceived_ = 0;
    uint64_t packetsProcessed_ = 0;
    uint64_t packetsRejected_ = 0;
    uint64_t bytesReceived_ = 0;
};

} // namespace udp_gateway