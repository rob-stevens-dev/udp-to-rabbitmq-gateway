#include "rabbitmq_integration/publisher.hpp"
#include "rabbitmq_integration/connection.hpp"
#include <spdlog/spdlog.h>

namespace rabbitmq_integration {

Publisher::Publisher(const PublisherConfig& config)
    : config_(config), connected_(false), confirmsEnabled_(false), 
      transactionActive_(false), publishingPaused_(false),
      localQueueSize_(0), localQueueEnabled_(true), asyncRunning_(false),
      nextDeliveryTag_(1) {
    
    spdlog::debug("Creating Publisher");
    
    // Initialize statistics
    stats_.publisherId = "pub_" + std::to_string(reinterpret_cast<uintptr_t>(this));
}

Publisher::~Publisher() {
    spdlog::debug("Destroying Publisher");
    stopAsyncPublishing();
    teardownConnection();
}

Publisher::Publisher(Publisher&& other) noexcept
    : config_(std::move(other.config_)),
      connection_(std::move(other.connection_)),
      channel_(std::move(other.channel_)),
      connected_(other.connected_.load()),
      confirmsEnabled_(other.confirmsEnabled_.load()),
      transactionActive_(other.transactionActive_.load()),
      publishingPaused_(other.publishingPaused_.load()),
      localQueue_(std::move(other.localQueue_)),
      localQueueSize_(other.localQueueSize_.load()),
      localQueueEnabled_(other.localQueueEnabled_.load()),
      asyncRunning_(other.asyncRunning_.load()),
      stats_(std::move(other.stats_)),
      errorCallback_(std::move(other.errorCallback_)),
      connectionCallback_(std::move(other.connectionCallback_)),
      returnCallback_(std::move(other.returnCallback_)),
      pendingConfirms_(std::move(other.pendingConfirms_)),
      nextDeliveryTag_(other.nextDeliveryTag_) {
    
    // Reset other object
    other.connected_ = false;
    other.confirmsEnabled_ = false;
    other.transactionActive_ = false;
    other.publishingPaused_ = false;
    other.localQueueSize_ = 0;
    other.localQueueEnabled_ = false;
    other.asyncRunning_ = false;
    other.nextDeliveryTag_ = 1;
}

Publisher& Publisher::operator=(Publisher&& other) noexcept {
    if (this != &other) {
        stopAsyncPublishing();
        teardownConnection();
        
        config_ = std::move(other.config_);
        connection_ = std::move(other.connection_);
        channel_ = std::move(other.channel_);
        connected_ = other.connected_.load();
        confirmsEnabled_ = other.confirmsEnabled_.load();
        transactionActive_ = other.transactionActive_.load();
        publishingPaused_ = other.publishingPaused_.load();
        localQueue_ = std::move(other.localQueue_);
        localQueueSize_ = other.localQueueSize_.load();
        localQueueEnabled_ = other.localQueueEnabled_.load();
        asyncRunning_ = other.asyncRunning_.load();
        stats_ = std::move(other.stats_);
        errorCallback_ = std::move(other.errorCallback_);
        connectionCallback_ = std::move(other.connectionCallback_);
        returnCallback_ = std::move(other.returnCallback_);
        pendingConfirms_ = std::move(other.pendingConfirms_);
        nextDeliveryTag_ = other.nextDeliveryTag_;
        
        // Reset other object
        other.connected_ = false;
        other.confirmsEnabled_ = false;
        other.transactionActive_ = false;
        other.publishingPaused_ = false;
        other.localQueueSize_ = 0;
        other.localQueueEnabled_ = false;
        other.asyncRunning_ = false;
        other.nextDeliveryTag_ = 1;
    }
    return *this;
}

bool Publisher::publish(const std::string& exchange, const std::string& routingKey, 
                       const Message& message) {
    return publish(exchange, routingKey, message, false, false);
}

bool Publisher::publish(const std::string& exchange, const std::string& routingKey, 
                       const Message& message, bool mandatory, bool immediate) {
    if (publishingPaused_) {
        spdlog::warn("Publishing is paused");
        return false;
    }
    
    if (!validateMessage(message)) {
        spdlog::error("Invalid message");
        return false;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    bool success = publishInternal(exchange, routingKey, message, mandatory, immediate);
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    updateStats(success, message.size(), duration);
    
    if (success) {
        spdlog::debug("Published message to exchange: {}, routing key: {}", exchange, routingKey);
    } else {
        spdlog::error("Failed to publish message to exchange: {}, routing key: {}", exchange, routingKey);
        handlePublishError(exchange, routingKey, "Publish failed");
    }
    
    return success;
}

bool Publisher::publishToInbound(const std::string& region, const std::string& imei, 
                                const Message& message) {
    std::string queueName = generateQueueName(region, imei, "inbound");
    return publish("", queueName, message);
}

bool Publisher::publishToInbound(const std::string& region, const std::string& imei, 
                                const nlohmann::json& data) {
    Message message(data);
    return publishToInbound(region, imei, message);
}

bool Publisher::publishToDebug(const std::string& region, const std::string& imei, 
                              const Message& message) {
    std::string queueName = generateQueueName(region, imei, "debug");
    return publish("", queueName, message);
}

bool Publisher::publishToDebug(const std::string& region, const std::string& imei, 
                              const nlohmann::json& data) {
    Message message(data);
    return publishToDebug(region, imei, message);
}

bool Publisher::publishToDiscard(const std::string& region, const std::string& imei, 
                                const Message& message, const std::string& reason) {
    Message discardMessage = message;
    discardMessage.setHeader("discard_reason", reason);
    discardMessage.setHeader("original_timestamp", std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    
    std::string queueName = generateQueueName(region, imei, "discard");
    return publish("", queueName, discardMessage);
}

bool Publisher::publishToDiscard(const std::string& region, const std::string& imei, 
                                const nlohmann::json& data, const std::string& reason) {
    Message message(data);
    return publishToDiscard(region, imei, message, reason);
}

bool Publisher::publishCommand(const std::string& region, const std::string& imei, 
                              const Message& command) {
    std::string queueName = generateQueueName(region, imei, "outbound");
    return publish("", queueName, command);
}

bool Publisher::publishCommand(const std::string& region, const std::string& imei, 
                              const nlohmann::json& command) {
    Message message(command);
    return publishCommand(region, imei, message);
}

bool Publisher::publishBatch(const std::string& exchange, const std::string& routingKey,
                            const std::vector<Message>& messages) {
    if (publishingPaused_) {
        spdlog::warn("Publishing is paused");
        return false;
    }
    
    bool allSuccess = true;
    
    // Begin transaction for batch
    if (config_.useTransactions) {
        beginTransaction();
    }
    
    for (const auto& message : messages) {
        if (!publish(exchange, routingKey, message)) {
            allSuccess = false;
            break;
        }
    }
    
    // Commit or rollback transaction
    if (config_.useTransactions) {
        if (allSuccess) {
            commitTransaction();
        } else {
            rollbackTransaction();
        }
    }
    
    spdlog::debug("Published batch of {} messages (success: {})", messages.size(), allSuccess);
    return allSuccess;
}

bool Publisher::publishBatch(const std::vector<std::tuple<std::string, std::string, Message>>& batch) {
    if (publishingPaused_) {
        spdlog::warn("Publishing is paused");
        return false;
    }
    
    bool allSuccess = true;
    
    if (config_.useTransactions) {
        beginTransaction();
    }
    
    for (const auto& [exchange, routingKey, message] : batch) {
        if (!publish(exchange, routingKey, message)) {
            allSuccess = false;
            break;
        }
    }
    
    if (config_.useTransactions) {
        if (allSuccess) {
            commitTransaction();
        } else {
            rollbackTransaction();
        }
    }
    
    spdlog::debug("Published batch of {} messages (success: {})", batch.size(), allSuccess);
    return allSuccess;
}

std::future<bool> Publisher::publishAsync(const std::string& exchange, const std::string& routingKey,
                                         const Message& message) {
    std::packaged_task<bool()> task([this, exchange, routingKey, message]() {
        return publish(exchange, routingKey, message);
    });
    
    auto future = task.get_future();
    
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        asyncTasks_.push(std::move(task));
    }
    
    asyncCondition_.notify_one();
    
    if (!asyncRunning_) {
        startAsyncPublishing();
    }
    
    return future;
}

std::future<bool> Publisher::publishToInboundAsync(const std::string& region, const std::string& imei,
                                                  const Message& message) {
    std::string queueName = generateQueueName(region, imei, "inbound");
    return publishAsync("", queueName, message);
}

bool Publisher::beginTransaction() {
    if (!connected_) {
        spdlog::error("Not connected - cannot begin transaction");
        return false;
    }
    
    if (transactionActive_) {
        spdlog::warn("Transaction already active");
        return true;
    }
    
    // Implementation would call channel->txSelect()
    transactionActive_ = true;
    spdlog::debug("Transaction started");
    return true;
}

bool Publisher::commitTransaction() {
    if (!transactionActive_) {
        spdlog::warn("No active transaction to commit");
        return false;
    }
    
    // Implementation would call channel->txCommit()
    transactionActive_ = false;
    stats_.transactionCount++;
    spdlog::debug("Transaction committed");
    return true;
}

bool Publisher::rollbackTransaction() {
    if (!transactionActive_) {
        spdlog::warn("No active transaction to rollback");
        return false;
    }
    
    // Implementation would call channel->txRollback()
    transactionActive_ = false;
    stats_.transactionRollbacks++;
    spdlog::debug("Transaction rolled back");
    return true;
}

bool Publisher::waitForConfirms(std::chrono::milliseconds timeout) {
    if (!confirmsEnabled_) {
        spdlog::warn("Publisher confirms not enabled");
        return true; // No confirms to wait for
    }
    
    // Implementation would wait for all pending confirms
    spdlog::debug("Waiting for confirms (timeout: {}ms)", timeout.count());
    return true;
}

bool Publisher::waitForConfirmsOrDie(std::chrono::milliseconds timeout) {
    if (!waitForConfirms(timeout)) {
        spdlog::error("Confirm timeout - some messages may not have been delivered");
        return false;
    }
    return true;
}

bool Publisher::isConnected() const {
    return connected_;
}

ConnectionState Publisher::getConnectionState() const {
    return connection_ ? connection_->getState() : ConnectionState::Disconnected;
}

PublisherStats Publisher::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void Publisher::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    auto publisherId = stats_.publisherId;
    stats_ = PublisherStats{};
    stats_.publisherId = publisherId;
}

const PublisherConfig& Publisher::getConfig() const {
    return config_;
}

bool Publisher::updateConfig(const PublisherConfig& config) {
    config_ = config;
    // TODO: Apply configuration changes to active connection
    return true;
}

size_t Publisher::getLocalQueueSize() const {
    return localQueueSize_;
}

bool Publisher::isLocalQueueEnabled() const {
    return localQueueEnabled_;
}

void Publisher::enableLocalQueue(bool enable) {
    localQueueEnabled_ = enable;
}

void Publisher::clearLocalQueue() {
    std::lock_guard<std::mutex> lock(localQueueMutex_);
    while (!localQueue_.empty()) {
        localQueue_.pop();
    }
    localQueueSize_ = 0;
}

void Publisher::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

void Publisher::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = std::move(callback);
}

void Publisher::setReturnCallback(std::function<void(const Message&, const std::string&)> callback) {
    returnCallback_ = std::move(callback);
}

bool Publisher::ping() {
    return connection_ && connection_->ping();
}

bool Publisher::isHealthy() const {
    return connected_ && connection_ && connection_->isHealthy();
}

void Publisher::pausePublishing() {
    publishingPaused_ = true;
    spdlog::info("Publishing paused");
}

void Publisher::resumePublishing() {
    publishingPaused_ = false;
    spdlog::info("Publishing resumed");
    
    // Process any queued messages
    processLocalQueue();
}

bool Publisher::isPublishingPaused() const {
    return publishingPaused_;
}

// Private methods
bool Publisher::initializeConnection() {
    if (connection_ && connection_->isConnected()) {
        return true;
    }
    
    try {
        connection_ = std::make_shared<Connection>(config_);
        if (!connection_->open()) {
            spdlog::error("Failed to open connection");
            return false;
        }
        
        connected_ = true;
        
        if (!initializeChannel()) {
            spdlog::error("Failed to initialize channel");
            return false;
        }
        
        spdlog::info("Publisher connection initialized");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize connection: {}", e.what());
        return false;
    }
}

void Publisher::teardownConnection() {
    connected_ = false;
    teardownChannel();
    
    if (connection_) {
        connection_->close();
        connection_.reset();
    }
}

bool Publisher::initializeChannel() {
    if (!connection_) {
        return false;
    }
    
    channel_ = connection_->createChannel();
    if (!channel_) {
        return false;
    }
    
    // Enable confirms if configured
    if (config_.confirmEnabled) {
        confirmsEnabled_ = true;
        // Implementation would call channel->enableConfirms()
    }
    
    return true;
}

void Publisher::teardownChannel() {
    confirmsEnabled_ = false;
    
    if (channel_) {
        // Implementation would close channel
        channel_.reset();
    }
}

bool Publisher::publishInternal(const std::string& exchange, const std::string& routingKey,
                               const Message& message, bool mandatory, bool immediate) {
    (void)mandatory;  // ADD THIS LINE
    (void)immediate;  // ADD THIS LINE
    
    if (!connected_) {
        if (localQueueEnabled_) {
            return publishToLocal(exchange, routingKey, message);
        } else {
            spdlog::error("Not connected and local queue disabled");
            return false;
        }
    }
    
    try {
        // Implementation would use channel->basicPublish()
        
        // Simulate successful publish
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Publish error: {}", e.what());
        
        if (localQueueEnabled_) {
            return publishToLocal(exchange, routingKey, message);
        }
        
        return false;
    }
}

bool Publisher::publishToLocal(const std::string& exchange, const std::string& routingKey,
                              const Message& message) {
    std::lock_guard<std::mutex> lock(localQueueMutex_);
    
    if (localQueue_.size() >= config_.localQueueSize) {
        spdlog::warn("Local queue full, dropping message");
        return false;
    }
    
    localQueue_.emplace(exchange, routingKey, message);
    localQueueSize_++;
    
    spdlog::debug("Message queued locally (queue size: {})", localQueueSize_.load());
    return true;
}

void Publisher::processLocalQueue() {
    if (!connected_ || !localQueueEnabled_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(localQueueMutex_);
    
    while (!localQueue_.empty()) {
        auto [exchange, routingKey, message] = localQueue_.front();
        localQueue_.pop();
        localQueueSize_--;
        
        // Try to publish queued message
        if (!publishInternal(exchange, routingKey, message, false, false)) {
            // Put it back in queue if failed
            localQueue_.emplace(exchange, routingKey, message);
            localQueueSize_++;
            break;
        }
    }
    
    if (localQueue_.empty()) {
        spdlog::info("Local queue processed successfully");
    }
}

std::string Publisher::generateQueueName(const std::string& region, const std::string& imei,
                                        const std::string& queueType) const {
    return region + "." + imei + "_" + queueType;
}

void Publisher::updateStats(bool success, size_t messageSize, 
                           std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    if (success) {
        stats_.messagesPublished++;
        stats_.bytesPublished += messageSize;
        stats_.lastPublish = std::chrono::system_clock::now();
    } else {
        stats_.messagesFailed++;
    }
    
    // Update timing stats
    if (duration > stats_.maxPublishTime) {
        stats_.maxPublishTime = duration;
    }
    
    if (stats_.avgPublishTime.count() == 0) {
        stats_.avgPublishTime = duration;
    } else {
        stats_.avgPublishTime = std::chrono::milliseconds(
            (stats_.avgPublishTime.count() + duration.count()) / 2);
    }
    
    stats_.localQueueSize.store(localQueueSize_);
    stats_.localQueueMaxSize = config_.localQueueSize;
}

void Publisher::handlePublishError(const std::string& exchange, const std::string& routingKey,
                                  const std::string& error) {
    std::string context = "Exchange: " + exchange + ", RoutingKey: " + routingKey;
    
    if (errorCallback_) {
        errorCallback_("PUBLISH_ERROR", error, context);
    }
}

void Publisher::onConnectionStateChanged(ConnectionState state, const std::string& reason) {
    connected_ = (state == ConnectionState::Connected);
    
    if (connected_) {
        processLocalQueue();
    }
    
    if (connectionCallback_) {
        connectionCallback_(connected_, reason);
    }
}

void Publisher::onPublishConfirm(uint64_t deliveryTag, bool ack) {
    std::lock_guard<std::mutex> lock(confirmMutex_);
    
    auto it = pendingConfirms_.find(deliveryTag);
    if (it != pendingConfirms_.end()) {
        it->second.set_value(ack);
        pendingConfirms_.erase(it);
        
        if (ack) {
            stats_.messagesConfirmed++;
        } else {
            stats_.messagesNacked++;
        }
    }
}

void Publisher::onPublishReturn(const Message& message, const std::string& replyText) {
    stats_.messagesReturned++;
    
    if (returnCallback_) {
        returnCallback_(message, replyText);
    }
}

void Publisher::asyncPublishThreadFunc() {
    spdlog::debug("Async publish thread started");
    
    while (asyncRunning_) {
        std::unique_lock<std::mutex> lock(asyncMutex_);
        
        asyncCondition_.wait(lock, [this] { 
            return !asyncTasks_.empty() || !asyncRunning_; 
        });
        
        while (!asyncTasks_.empty() && asyncRunning_) {
            auto task = std::move(asyncTasks_.front());
            asyncTasks_.pop();
            lock.unlock();
            
            // Execute task
            task();
            
            lock.lock();
        }
    }
    
    spdlog::debug("Async publish thread ended");
}

void Publisher::startAsyncPublishing() {
    if (asyncRunning_) {
        return;
    }
    
    asyncRunning_ = true;
    asyncPublishThread_ = std::make_unique<std::thread>(&Publisher::asyncPublishThreadFunc, this);
}

void Publisher::stopAsyncPublishing() {
    asyncRunning_ = false;
    asyncCondition_.notify_all();
    
    if (asyncPublishThread_ && asyncPublishThread_->joinable()) {
        asyncPublishThread_->join();
    }
    
    asyncPublishThread_.reset();
}

bool Publisher::validateMessage(const Message& message) const {
    return !message.empty() && message.isValid();
}

bool Publisher::validateExchange(const std::string& exchange) const {
    return exchange.length() <= 255;
}

bool Publisher::validateRoutingKey(const std::string& routingKey) const {
    return routingKey.length() <= 255;
}

void Publisher::cleanup() {
    stopAsyncPublishing();
    teardownConnection();
    clearLocalQueue();
}

// Factory implementations
std::unique_ptr<Publisher> PublisherFactory::createTelemetryPublisher(const PublisherConfig& config) {
    return std::make_unique<Publisher>(config);
}

std::unique_ptr<Publisher> PublisherFactory::createCommandPublisher(const PublisherConfig& config) {
    return std::make_unique<Publisher>(config);
}

std::unique_ptr<Publisher> PublisherFactory::createDebugPublisher(const PublisherConfig& config) {
    auto optimizedConfig = config;
    optimizedConfig.persistentMessages = false; // Debug messages don't need persistence
    return std::make_unique<Publisher>(optimizedConfig);
}

std::unique_ptr<Publisher> PublisherFactory::createHighThroughputPublisher(const ConnectionConfig& connConfig) {
    PublisherConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.confirmEnabled = false; // Disable confirms for throughput
    config.batchSize = 1000;
    config.localQueueSize = 50000;
    return std::make_unique<Publisher>(config);
}

std::unique_ptr<Publisher> PublisherFactory::createReliablePublisher(const ConnectionConfig& connConfig) {
    PublisherConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.confirmEnabled = true;
    config.useTransactions = true;
    config.persistentMessages = true;
    return std::make_unique<Publisher>(config);
}

std::unique_ptr<Publisher> PublisherFactory::createLowLatencyPublisher(const ConnectionConfig& connConfig) {
    PublisherConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.confirmEnabled = false;
    config.batchSize = 1;
    config.localQueueSize = 100;
    return std::make_unique<Publisher>(config);
}

PublisherConfig PublisherFactory::createOptimizedConfig(const ConnectionConfig& connConfig,
                                                       const std::string& optimization) {
    PublisherConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    
    if (optimization == "high_throughput") {
        config.confirmEnabled = false;
        config.batchSize = 1000;
        config.localQueueSize = 50000;
    } else if (optimization == "reliable") {
        config.confirmEnabled = true;
        config.useTransactions = true;
        config.persistentMessages = true;
    } else if (optimization == "low_latency") {
        config.confirmEnabled = false;
        config.batchSize = 1;
        config.localQueueSize = 100;
    }
    
    return config;
}

} // namespace rabbitmq_integration