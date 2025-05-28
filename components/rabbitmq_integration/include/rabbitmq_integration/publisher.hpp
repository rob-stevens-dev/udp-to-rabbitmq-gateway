#pragma once

#include "types.hpp"
#include "message.hpp"
#include "connection.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <future>

namespace rabbitmq_integration {

// Forward declaration
class Channel;

class Publisher {
public:
    // Constructor
    explicit Publisher(const PublisherConfig& config);
    
    // Destructor
    ~Publisher();
    
    // Move semantics
    Publisher(Publisher&& other) noexcept;
    Publisher& operator=(Publisher&& other) noexcept;
    
    // Non-copyable
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    
    // Basic publishing methods
    bool publish(const std::string& exchange, const std::string& routingKey, 
                 const Message& message);
    
    bool publish(const std::string& exchange, const std::string& routingKey, 
                 const Message& message, bool mandatory, bool immediate = false);
    
    // Device-specific publishing methods
    bool publishToInbound(const std::string& region, const std::string& imei, 
                          const Message& message);
    bool publishToInbound(const std::string& region, const std::string& imei, 
                          const nlohmann::json& data);
    
    bool publishToDebug(const std::string& region, const std::string& imei, 
                        const Message& message);
    bool publishToDebug(const std::string& region, const std::string& imei, 
                        const nlohmann::json& data);
    
    bool publishToDiscard(const std::string& region, const std::string& imei, 
                          const Message& message, const std::string& reason);
    bool publishToDiscard(const std::string& region, const std::string& imei, 
                          const nlohmann::json& data, const std::string& reason);
    
    bool publishCommand(const std::string& region, const std::string& imei, 
                        const Message& command);
    bool publishCommand(const std::string& region, const std::string& imei, 
                        const nlohmann::json& command);
    
    // Batch publishing
    bool publishBatch(const std::string& exchange, const std::string& routingKey,
                      const std::vector<Message>& messages);
    
    bool publishBatch(const std::vector<std::tuple<std::string, std::string, Message>>& batch);
    
    // Asynchronous publishing
    std::future<bool> publishAsync(const std::string& exchange, const std::string& routingKey,
                                   const Message& message);
    
    std::future<bool> publishToInboundAsync(const std::string& region, const std::string& imei,
                                            const Message& message);
    
    // Transaction support
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    
    // Publisher confirms
    bool waitForConfirms(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    bool waitForConfirmsOrDie(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Connection state
    bool isConnected() const;
    ConnectionState getConnectionState() const;
    
    // Statistics
    PublisherStats getStats() const;
    void resetStats();
    
    // Configuration
    const PublisherConfig& getConfig() const;
    bool updateConfig(const PublisherConfig& config);
    
    // Local queue management (for offline publishing)
    size_t getLocalQueueSize() const;
    bool isLocalQueueEnabled() const;
    void enableLocalQueue(bool enable = true);
    void clearLocalQueue();
    
    // Callback management
    void setErrorCallback(ErrorCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setReturnCallback(std::function<void(const Message&, const std::string&)> callback);
    
    // Health check
    bool ping();
    bool isHealthy() const;
    
    // Flow control
    void pausePublishing();
    void resumePublishing();
    bool isPublishingPaused() const;
    
private:
    // Configuration
    PublisherConfig config_;
    
    // Connection management
    std::shared_ptr<Connection> connection_;
    std::shared_ptr<Channel> channel_;
    
    // Publisher state
    std::atomic<bool> connected_;
    std::atomic<bool> confirmsEnabled_;
    std::atomic<bool> transactionActive_;
    std::atomic<bool> publishingPaused_;
    
    // Local queue for offline publishing
    mutable std::mutex localQueueMutex_;
    std::queue<std::tuple<std::string, std::string, Message>> localQueue_;
    std::atomic<size_t> localQueueSize_;
    std::atomic<bool> localQueueEnabled_;
    
    // Asynchronous publishing
    std::unique_ptr<std::thread> asyncPublishThread_;
    std::queue<std::packaged_task<bool()>> asyncTasks_;
    mutable std::mutex asyncMutex_;
    std::condition_variable asyncCondition_;
    std::atomic<bool> asyncRunning_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    PublisherStats stats_;
    
    // Callbacks
    ErrorCallback errorCallback_;
    ConnectionCallback connectionCallback_;
    std::function<void(const Message&, const std::string&)> returnCallback_;
    
    // Confirmation tracking
    mutable std::mutex confirmMutex_;
    std::map<uint64_t, std::promise<bool>> pendingConfirms_;
    uint64_t nextDeliveryTag_;
    
    // Private methods
    bool initializeConnection();
    void teardownConnection();
    bool initializeChannel();
    void teardownChannel();
    
    // Publishing implementation
    bool publishInternal(const std::string& exchange, const std::string& routingKey,
                        const Message& message, bool mandatory, bool immediate);
    
    bool publishToLocal(const std::string& exchange, const std::string& routingKey,
                       const Message& message);
    
    void processLocalQueue();
    
    // Queue name generation
    std::string generateQueueName(const std::string& region, const std::string& imei,
                                 const std::string& queueType) const;
    
    // Statistics updates
    void updateStats(bool success, size_t messageSize, 
                    std::chrono::milliseconds duration);
    
    // Error handling
    void handlePublishError(const std::string& exchange, const std::string& routingKey,
                           const std::string& error);
    void onConnectionStateChanged(ConnectionState state, const std::string& reason);
    
    // Confirmation handling
    void onPublishConfirm(uint64_t deliveryTag, bool ack);
    void onPublishReturn(const Message& message, const std::string& replyText);
    
    // Async publishing
    void asyncPublishThreadFunc();
    void startAsyncPublishing();
    void stopAsyncPublishing();
    
    // Validation
    bool validateMessage(const Message& message) const;
    bool validateExchange(const std::string& exchange) const;
    bool validateRoutingKey(const std::string& routingKey) const;
    
    // Cleanup
    void cleanup();
};

// Publisher factory for creating specialized publishers
class PublisherFactory {
public:
    // Create standard publishers
    static std::unique_ptr<Publisher> createTelemetryPublisher(const PublisherConfig& config);
    static std::unique_ptr<Publisher> createCommandPublisher(const PublisherConfig& config);
    static std::unique_ptr<Publisher> createDebugPublisher(const PublisherConfig& config);
    
    // Create optimized publishers
    static std::unique_ptr<Publisher> createHighThroughputPublisher(const ConnectionConfig& connConfig);
    static std::unique_ptr<Publisher> createReliablePublisher(const ConnectionConfig& connConfig);
    static std::unique_ptr<Publisher> createLowLatencyPublisher(const ConnectionConfig& connConfig);
    
    // Configuration helpers
    static PublisherConfig createOptimizedConfig(const ConnectionConfig& connConfig,
                                               const std::string& optimization);
};

// Batch publisher for high-throughput scenarios
class BatchPublisher {
public:
    explicit BatchPublisher(const PublisherConfig& config, size_t batchSize = 100);
    ~BatchPublisher();
    
    // Non-copyable
    BatchPublisher(const BatchPublisher&) = delete;
    BatchPublisher& operator=(const BatchPublisher&) = delete;
    
    // Add messages to batch
    bool addMessage(const std::string& exchange, const std::string& routingKey,
                   const Message& message);
    
    bool addTelemetryMessage(const std::string& region, const std::string& imei,
                            const Message& message);
    
    // Flush batch
    bool flush();
    void autoFlush(std::chrono::milliseconds interval);
    
    // Control
    bool start();
    void stop();
    bool isRunning() const;
    
    // Statistics
    PublisherStats getStats() const;
    size_t getCurrentBatchSize() const;
    size_t getBatchesPublished() const;
    
private:
    PublisherConfig config_;
    size_t batchSize_;
    std::atomic<bool> running_;
    
    std::unique_ptr<Publisher> publisher_;
    
    // Batch management
    std::vector<std::tuple<std::string, std::string, Message>> currentBatch_;
    mutable std::mutex batchMutex_;
    
    // Auto-flush
    std::unique_ptr<std::thread> flushThread_;
    std::condition_variable flushCondition_;
    std::chrono::milliseconds flushInterval_;
    
    // Statistics
    std::atomic<size_t> batchesPublished_;
    
    void flushThreadFunc();
    bool shouldFlush() const;
};

// Publisher pool for load distribution
class PublisherPool {
public:
    explicit PublisherPool(const PublisherConfig& config, size_t poolSize = 4);
    ~PublisherPool();
    
    // Non-copyable
    PublisherPool(const PublisherPool&) = delete;
    PublisherPool& operator=(const PublisherPool&) = delete;
    
    // Publishing methods
    bool publish(const std::string& exchange, const std::string& routingKey,
                 const Message& message);
    
    bool publishToInbound(const std::string& region, const std::string& imei,
                          const Message& message);
    
    // Pool management
    bool start();
    void stop();
    bool isRunning() const;
    
    // Statistics
    PublisherStats getAggregatedStats() const;
    std::vector<PublisherStats> getIndividualStats() const;
    
    // Health monitoring
    void performHealthCheck();
    
private:
    PublisherConfig config_;
    size_t poolSize_;
    std::atomic<bool> running_;
    std::atomic<size_t> roundRobinIndex_;
    
    std::vector<std::unique_ptr<Publisher>> publishers_;
    mutable std::mutex poolMutex_;
    
    void initializePool();
    void cleanupPool();
    Publisher* getNextPublisher();
};

} // namespace rabbitmq_integration