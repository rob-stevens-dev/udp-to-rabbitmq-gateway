#pragma once

#include "udp_gateway/types.hpp"
#include "udp_gateway/session.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <functional>

// Forward declarations for component interfaces
namespace protocol_parser { class IProtocolParser; }
namespace redis_deduplication { class IDeduplicationService; }
namespace rabbitmq_integration { class IMessagePublisher; }
namespace device_manager { class IDeviceManager; }
namespace monitoring_system { class IMetricsCollector; }

namespace udp_gateway {

// Custom hash function for UDP endpoints
struct UdpEndpointHash {
    std::size_t operator()(const boost::asio::ip::udp::endpoint& endpoint) const {
        std::size_t h1 = std::hash<std::string>{}(endpoint.address().to_string());
        std::size_t h2 = std::hash<unsigned short>{}(endpoint.port());
        return h1 ^ (h2 << 1); // Simple combine
    }
};

// Custom equality function for UDP endpoints
struct UdpEndpointEqual {
    bool operator()(const boost::asio::ip::udp::endpoint& lhs, 
                    const boost::asio::ip::udp::endpoint& rhs) const {
        return lhs.address() == rhs.address() && lhs.port() == rhs.port();
    }
};

/**
 * @class Gateway
 * @brief Main UDP Gateway component that handles device connections
 * 
 * This class is the central component of the UDP Gateway. It manages
 * UDP connections, processes device messages, and coordinates with
 * other system components for message handling.
 */
class Gateway : public ISessionHandler, 
                public std::enable_shared_from_this<Gateway> {
public:
    /**
     * @brief Configuration for the UDP Gateway
     */
    struct Config {
        // Network configuration
        std::string listenAddress = "0.0.0.0";
        uint16_t listenPort = 8125;
        bool enableIPv6 = false;
        size_t maxMessageSize = 8192;
        
        // Threading configuration
        size_t numWorkerThreads = 0;  // 0 means use hardware concurrency
        
        // Regional configuration
        std::string regionCode = "na";  // Default to North America
        
        // Session configuration
        bool enableDebugging = false;
        bool enableAcknowledgments = true;
        std::chrono::milliseconds processingTimeout{500};
        
        // Rate limiting configuration
        double defaultMessagesPerSecond = 10.0;
        size_t defaultBurstSize = 20;
        
        // Queue configuration
        std::string queuePrefix = "";  // Optional prefix for all queues
        
        // Metrics
        bool enableMetrics = true;
        std::chrono::seconds metricsInterval{10};
    };

    /**
     * @brief Construct a new Gateway
     * 
     * @param config Gateway configuration
     */
    explicit Gateway(const Config& config);

    /**
     * @brief Construct a new Gateway with custom component implementations
     * 
     * @param config Gateway configuration
     * @param protocolParser Custom protocol parser implementation
     * @param deduplicationService Custom deduplication service implementation
     * @param messagePublisher Custom message publisher implementation
     * @param deviceManager Custom device manager implementation
     * @param rateLimiter Custom rate limiter implementation
     * @param metricsCollector Custom metrics collector implementation
     */
    Gateway(
        const Config& config,
        std::shared_ptr<protocol_parser::IProtocolParser> protocolParser,
        std::shared_ptr<redis_deduplication::IDeduplicationService> deduplicationService,
        std::shared_ptr<rabbitmq_integration::IMessagePublisher> messagePublisher,
        std::shared_ptr<device_manager::IDeviceManager> deviceManager,
        std::shared_ptr<IRateLimiter> rateLimiter,
        std::shared_ptr<monitoring_system::IMetricsCollector> metricsCollector
    );

    /**
     * @brief Destroy the Gateway
     */
    ~Gateway();

    /**
     * @brief Start the gateway (non-blocking)
     * 
     * @return VoidResult Result indicating success or failure
     */
    VoidResult start();

    /**
     * @brief Stop the gateway
     */
    void stop();

    /**
     * @brief Check if the gateway is running
     * 
     * @return true if the gateway is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Get gateway metrics
     * 
     * @return GatewayMetrics Current metrics
     */
    GatewayMetrics getMetrics() const;

    /**
     * @brief Get the number of active connections
     * 
     * @return size_t The number of active connections
     */
    size_t getActiveConnectionCount() const;

    // Implementation of ISessionHandler interface
    VoidResult handleMessage(const DeviceMessage& message) override;
    void handleError(const IMEI& deviceId, ErrorCode errorCode, const std::string& errorMessage) override;
    std::vector<DeviceCommand> getPendingCommands(const IMEI& deviceId) override;

private:
    // Initialize the gateway components
    void initialize();

    // Set up the UDP socket
    void setupSocket();

    // Start accepting connections
    void startAccept();

    // Handle a new UDP connection
    void handleAccept(const boost::system::error_code& error, 
                     const boost::asio::ip::udp::endpoint& endpoint);

    // Thread function for io_context workers
    void workerThread();

    // Update metrics periodically
    void updateMetrics();

    // Create queue name based on region and IMEI
    std::string createQueueName(const std::string& imei, QueueType queueType) const;

    // Core members
    Config config_;
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::asio::io_context::work> workGuard_;
    boost::asio::ip::udp::socket socket_;
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> running_{false};
    
    // Active sessions - using our custom hash and equality functions
    std::unordered_map<
        boost::asio::ip::udp::endpoint, 
        std::shared_ptr<Session>, 
        UdpEndpointHash,
        UdpEndpointEqual
    > activeSessions_;
    mutable std::mutex sessionsMutex_;

    // Component dependencies
    std::shared_ptr<protocol_parser::IProtocolParser> protocolParser_;
    std::shared_ptr<redis_deduplication::IDeduplicationService> deduplicationService_;
    std::shared_ptr<rabbitmq_integration::IMessagePublisher> messagePublisher_;
    std::shared_ptr<device_manager::IDeviceManager> deviceManager_;
    std::shared_ptr<IRateLimiter> rateLimiter_;
    std::shared_ptr<monitoring_system::IMetricsCollector> metricsCollector_;

    // Metrics
    GatewayMetrics metrics_;
    mutable std::mutex metricsMutex_;
    std::unique_ptr<boost::asio::steady_timer> metricsTimer_;
};

} // namespace udp_gateway