#pragma once

#include "types.hpp"
#include "message.hpp"
#include "connection.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <map>
#include <functional>

namespace rabbitmq_integration {

// Forward declaration
class Channel;

class Consumer {
public:
    // Constructor
    explicit Consumer(const ConsumerConfig& config);
    
    // Destructor
    ~Consumer();
    
    // Move semantics
    Consumer(Consumer&& other) noexcept;
    Consumer& operator=(Consumer&& other) noexcept;
    
    // Non-copyable
    Consumer(const Consumer&) = delete;
    Consumer& operator=(const Consumer&) = delete;
    
    // Basic consumption methods
    bool startConsuming(const std::string& queueName,
                       MessageCallback callback);
    
    bool startConsuming(const std::string& queueName,
                       const std::string& consumerTag,
                       MessageCallback callback);
    
    bool startConsuming(const std::string& queueName,
                       const std::string& consumerTag,
                       MessageCallback callback,
                       const std::string& exchangeName,
                       const std::string& routingKey = "");
    
    // Advanced consumption with custom settings
    bool startConsuming(const std::string& queueName,
                       MessageCallback callback,
                       const ConsumerConfig& customConfig);
    
    // Stop consumption
    void stopConsuming();
    bool stopConsuming(const std::string& queueName);
    
    // Message retrieval (polling mode)
    std::optional<std::pair<Message, uint64_t>> getMessage(const std::string& queueName,
                                                          bool noAck = false);
    
    // Manual acknowledgment methods
    bool acknowledge(uint64_t deliveryTag);
    bool acknowledgeMultiple(uint64_t deliveryTag);
    bool reject(uint64_t deliveryTag, bool requeue = true);
    bool rejectMultiple(uint64_t deliveryTag, bool requeue = true);
    bool nack(uint64_t deliveryTag, bool multiple = false, bool requeue = true);
    
    // Connection state
    bool isConnected() const;
    ConnectionState getConnectionState() const;
    
    // Statistics
    ConsumerStats getStats() const;
    ConsumerStats getStatsForQueue(const std::string& queueName) const;
    void resetStats();
    
    // Configuration
    const ConsumerConfig& getConfig() const;
    bool updateConfig(const ConsumerConfig& config);
    
    // Callback management
    void setErrorCallback(ErrorCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    
    // Consumer information
    struct ConsumerInfo {
        std::string queueName;
        std::string consumerTag;
        MessageCallback callback;
        std::shared_ptr<Channel> channel;
        bool active = false;  // Change from std::atomic<bool> to regular bool
        ConsumerStats stats;
        std::chrono::system_clock::time_point lastActivity;
        
        // Constructor
        ConsumerInfo(const std::string& queue, const std::string& tag, 
                    MessageCallback cb, std::shared_ptr<Channel> ch)
            : queueName(queue), consumerTag(tag), callback(std::move(cb)), 
              channel(std::move(ch)), active(false) {
            lastActivity = std::chrono::system_clock::now();
        }
        
        // Move constructor
        ConsumerInfo(ConsumerInfo&& other) noexcept
            : queueName(std::move(other.queueName)),
              consumerTag(std::move(other.consumerTag)),
              callback(std::move(other.callback)),
              channel(std::move(other.channel)),
              active(other.active),
              stats(std::move(other.stats)),
              lastActivity(other.lastActivity) {
        }
        
        // Move assignment
        ConsumerInfo& operator=(ConsumerInfo&& other) noexcept {
            if (this != &other) {
                queueName = std::move(other.queueName);
                consumerTag = std::move(other.consumerTag);
                callback = std::move(other.callback);
                channel = std::move(other.channel);
                active = other.active;
                stats = std::move(other.stats);
                lastActivity = other.lastActivity;
            }
            return *this;
        }
        
        // Delete copy operations
        ConsumerInfo(const ConsumerInfo&) = delete;
        ConsumerInfo& operator=(const ConsumerInfo&) = delete;
    };
    
    // Get active consumers
    std::vector<std::string> getActiveConsumers() const;
    std::vector<ConsumerInfo> getConsumerInfo() const;
    
private:
    // Configuration
    ConsumerConfig config_;
    
    // Connection management
    std::shared_ptr<Connection> connection_;
    
    // Consumer state
    std::atomic<bool> running_;
    std::atomic<bool> stopping_;
    
    // Consumer management
    mutable std::mutex consumersMutex_;
    std::map<std::string, std::unique_ptr<ConsumerInfo>> consumers_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    ConsumerStats aggregatedStats_;
    
    // Threading
    std::vector<std::unique_ptr<std::thread>> consumerThreads_;
    
    // Callbacks
    ErrorCallback errorCallback_;
    ConnectionCallback connectionCallback_;
    
    // Private methods
    bool initializeConnection();
    void teardownConnection();
    bool createConsumer(const std::string& queueName,
                       const std::string& consumerTag,
                       MessageCallback callback,
                       std::shared_ptr<Channel> channel);
    
    void consumerThreadFunc(std::unique_ptr<ConsumerInfo> info);
    bool processMessage(ConsumerInfo& info, const Message& message, uint64_t deliveryTag);
    void updateAggregatedStats();
    void updateStatsForConsumer(ConsumerInfo& info, bool messageProcessed, 
                               std::chrono::milliseconds processingTime);
    
    // Error handling
    void onConnectionStateChanged(ConnectionState state, const std::string& reason);
    void handleConsumerError(const std::string& consumerTag, 
                           const std::string& errorCode, 
                           const std::string& errorMessage);
    
    // Cleanup
    void cleanup();
    void cleanupConsumer(const std::string& queueName);
    
    // Validation
    bool validateQueue(const std::string& queueName) const;
    bool isValidConsumerTag(const std::string& consumerTag) const;
    std::string generateConsumerTag(const std::string& queueName) const;
};

// Consumer factory for creating specialized consumers
class ConsumerFactory {
public:
    // Create standard consumers
    static std::unique_ptr<Consumer> createCommandConsumer(const ConsumerConfig& config);
    static std::unique_ptr<Consumer> createTelemetryConsumer(const ConsumerConfig& config);
    static std::unique_ptr<Consumer> createDebugConsumer(const ConsumerConfig& config);
    
    // Create optimized consumers
    static std::unique_ptr<Consumer> createHighThroughputConsumer(const ConnectionConfig& connConfig);
    static std::unique_ptr<Consumer> createReliableConsumer(const ConnectionConfig& connConfig);
    static std::unique_ptr<Consumer> createLowLatencyConsumer(const ConnectionConfig& connConfig);
    
    // Configuration helpers
    static ConsumerConfig createOptimizedConfig(const ConnectionConfig& connConfig,
                                              const std::string& optimization);
};

// Multi-queue consumer for handling multiple queues in a single consumer
class MultiQueueConsumer {
public:
    explicit MultiQueueConsumer(const ConsumerConfig& config);
    ~MultiQueueConsumer();
    
    // Non-copyable
    MultiQueueConsumer(const MultiQueueConsumer&) = delete;
    MultiQueueConsumer& operator=(const MultiQueueConsumer&) = delete;
    
    // Queue management
    bool addDevice(const std::string& region, const std::string& imei,
                   MessageCallback callback);
    bool removeDevice(const std::string& region, const std::string& imei);
    
    // Generic queue operations
    bool addQueue(const std::string& queueName, MessageCallback callback);
    bool removeQueue(const std::string& queueName);
    
    // Control
    bool start();
    void stop();
    bool isRunning() const;
    
    // Statistics
    ConsumerStats getAggregatedStats() const;
    std::map<std::string, ConsumerStats> getPerQueueStats() const;
    
    // Configuration
    void updateConfig(const ConsumerConfig& config);
    
private:
    ConsumerConfig config_;
    std::atomic<bool> running_;
    
    // Queue callbacks
    std::map<std::string, MessageCallback> queueCallbacks_;
    
    // Individual consumers
    std::vector<std::unique_ptr<Consumer>> consumers_;
    
    mutable std::mutex mutex_;
};

// Consumer pool for high-throughput scenarios
class ConsumerPool {
public:
    explicit ConsumerPool(const ConsumerConfig& config, size_t poolSize = 4);
    ~ConsumerPool();
    
    // Non-copyable
    ConsumerPool(const ConsumerPool&) = delete;
    ConsumerPool& operator=(const ConsumerPool&) = delete;
    
    // Pool management
    bool start();
    void stop();
    bool isRunning() const;
    
    // Consumer management
    bool addConsumer(const std::string& queueName, MessageCallback callback);
    bool removeConsumer(const std::string& queueName);
    
    // Statistics
    ConsumerStats getAggregatedStats() const;
    std::vector<ConsumerStats> getIndividualStats() const;
    
    // Health monitoring
    void performHealthCheck();
    
private:
    ConsumerConfig config_;
    size_t poolSize_;
    std::atomic<bool> running_;
    
    std::vector<std::unique_ptr<Consumer>> consumers_;
    mutable std::mutex poolMutex_;
    
    void initializePool();
    void cleanupPool();
};

} // namespace rabbitmq_integration