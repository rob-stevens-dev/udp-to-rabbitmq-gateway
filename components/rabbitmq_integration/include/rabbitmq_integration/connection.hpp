#pragma once

#include "types.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <chrono>
#include <amqp.h>

namespace rabbitmq_integration {

// Forward declarations
class Channel;

class Connection {
public:
    // Constructors and destructor
    explicit Connection(const ConnectionConfig& config);
    Connection(const Connection&) = delete;
    Connection(Connection&& other) noexcept;
    ~Connection();

    // Assignment operators
    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&& other) noexcept;

    // Connection management
    bool open();
    void close();
    bool isConnected() const;
    bool isOpen() const;
    ConnectionState getState() const;

    // Channel management
    std::shared_ptr<Channel> createChannel();
    std::shared_ptr<Channel> createChannel(int channelId);
    bool closeChannel(int channelId);
    size_t getChannelCount() const;

    // Connection properties
    const ConnectionConfig& getConfig() const;
    std::string getServerProperties() const;
    std::tuple<int, int, int> getServerVersion() const;

    // Error handling
    std::string getLastError() const;
    void setErrorCallback(ErrorCallback callback);

    // Statistics
    ConnectionStats getStats() const;

    // Health check
    bool performHealthCheck();
    bool ping();
    bool isHealthy() const;

    // Advanced features
    void enableHeartbeat(std::chrono::seconds interval);
    void disableHeartbeat();
    bool isHeartbeatEnabled() const;

    // Low-level access (use with caution)
    amqp_connection_state_t getNativeHandle() const;

private:
    // Configuration
    ConnectionConfig config_;
    
    // AMQP connection state
    amqp_connection_state_t connection_ = nullptr;
    amqp_socket_t* socket_ = nullptr;
    
    // Connection state
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> isConnected_{false};
    
    // Channel management
    std::map<int, std::weak_ptr<Channel>> channels_;
    std::mutex channelsMutex_;
    int nextChannelId_ = 1;
    
    // Error handling
    mutable std::mutex errorMutex_;
    std::string lastError_;
    ErrorCallback errorCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    ConnectionStats stats_;
    
    // Heartbeat
    std::atomic<bool> heartbeatEnabled_{false};
    std::chrono::seconds heartbeatInterval_{60};
    std::thread heartbeatThread_;
    std::atomic<bool> stopHeartbeat_{false};
    
    // Internal methods
    bool initializeConnection();
    void teardownConnection();
    bool performHandshake();
    bool authenticateConnection();
    void updateConnectionState(ConnectionState newState);
    void setError(const std::string& error);
    void cleanupDeadChannels();
    void startHeartbeatThread();
    void stopHeartbeatThread();
    void heartbeatLoop();
    std::string getAmqpErrorString(amqp_rpc_reply_t reply) const;
    void cleanupChannels();
};

// Channel class declaration
class Channel {
public:
    // Constructors and destructor
    explicit Channel(std::shared_ptr<Connection> connection, int channelId);
    Channel(const Channel&) = delete;
    Channel(Channel&& other) noexcept;
    ~Channel();

    // Assignment operators
    Channel& operator=(const Channel&) = delete;
    Channel& operator=(Channel&& other) noexcept;

    // Channel management
    bool open();
    void close();
    bool isOpen() const;
    int getChannelId() const;

    // Exchange operations
    bool declareExchange(const std::string& name, const std::string& type,
                        bool durable = true, bool autoDelete = false,
                        const std::map<std::string, std::string>& arguments = {});
    bool deleteExchange(const std::string& name, bool ifUnused = false);
    bool exchangeExists(const std::string& name);

    // Queue operations
    bool declareQueue(const std::string& name, bool durable = true,
                     bool exclusive = false, bool autoDelete = false,
                     const std::map<std::string, std::string>& arguments = {});
    bool deleteQueue(const std::string& name, bool ifUnused = false, bool ifEmpty = false);
    bool purgeQueue(const std::string& name);
    bool queueExists(const std::string& name);

    // Binding operations
    bool bindQueue(const std::string& queue, const std::string& exchange,
                   const std::string& routingKey,
                   const std::map<std::string, std::string>& arguments = {});
    bool unbindQueue(const std::string& queue, const std::string& exchange,
                     const std::string& routingKey,
                     const std::map<std::string, std::string>& arguments = {});

    // Publishing
    bool basicPublish(const std::string& exchange, const std::string& routingKey,
                     const std::vector<uint8_t>& body, bool mandatory = false,
                     bool immediate = false, const MessageProperties& properties = {});

    // Transaction support
    bool txSelect();
    bool txCommit();
    bool txRollback();

    // Publisher confirms
    bool confirmSelect();
    bool waitForConfirms(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    // Consumer operations
    std::string basicConsume(const std::string& queue, const std::string& consumerTag = "",
                            bool noLocal = false, bool noAck = false, bool exclusive = false,
                            const std::map<std::string, std::string>& arguments = {});
    bool basicCancel(const std::string& consumerTag);
    bool basicAck(uint64_t deliveryTag, bool multiple = false);
    bool basicNack(uint64_t deliveryTag, bool multiple = false, bool requeue = true);
    bool basicReject(uint64_t deliveryTag, bool requeue = true);

    // Quality of Service
    bool basicQos(uint16_t prefetchCount, uint32_t prefetchSize = 0, bool global = false);

    // Error handling
    std::string getLastError() const;

    // Low-level access
    amqp_connection_state_t getConnection() const;

private:
    // Connection reference
    std::shared_ptr<Connection> connection_;
    
    // Channel properties
    int channelId_;
    std::atomic<bool> isOpen_{false};
    
    // Error handling
    mutable std::mutex errorMutex_;
    std::string lastError_;
    
    // Internal methods
    void setError(const std::string& error);
    std::string getAmqpErrorString(amqp_rpc_reply_t reply) const;
};

// Connection Pool for managing multiple connections
class ConnectionPool {
public:
    // Constructor and destructor
    explicit ConnectionPool(const ConnectionConfig& config, size_t poolSize = 10);
    ~ConnectionPool();

    // Pool management
    bool initialize();
    void shutdown();
    bool isRunning() const;

    // Connection acquisition
    std::shared_ptr<Connection> acquire();
    void release(std::shared_ptr<Connection> connection);

    // Pool statistics
    size_t getPoolSize() const;
    size_t getAvailableConnections() const;
    size_t getActiveConnections() const;

    // Health monitoring
    void performHealthCheck();
    void replaceUnhealthyConnections();

private:
    // Configuration
    ConnectionConfig config_;
    
    // AMQP connection state
    amqp_connection_state_t connection_ = nullptr;
    amqp_socket_t* socket_ = nullptr;
    
    // Connection state
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> isConnected_{false};
    
    // Channel management
    std::map<int, std::weak_ptr<Channel>> channels_;
    mutable std::mutex channelsMutex_;  // Add this mutex
    int nextChannelId_ = 1;
    
    // Error handling
    mutable std::mutex errorMutex_;
    std::string lastError_;
    ErrorCallback errorCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    ConnectionStats stats_;
    
    // Heartbeat (these are used in implementation)
    std::atomic<bool> heartbeatEnabled_{false};
    std::chrono::seconds heartbeatInterval_{60};
    std::thread heartbeatThread_;
    std::atomic<bool> stopHeartbeat_{false};
    std::chrono::system_clock::time_point lastHeartbeat_;
    
    // Add missing mutex for general operations
    mutable std::mutex mutex_;
    
    // Internal methods (these need to be declared)
    bool initializeConnection();
    void teardownConnection();
    bool performHandshake();
    bool authenticateConnection();
    void updateConnectionState(ConnectionState newState);
    void setError(const std::string& error);
    void cleanupDeadChannels();
    void startHeartbeatThread();
    void stopHeartbeatThread();
    void heartbeatLoop();
    std::string getAmqpErrorString(amqp_rpc_reply_t reply) const;
    void cleanupChannels();
};

} // namespace rabbitmq_integration