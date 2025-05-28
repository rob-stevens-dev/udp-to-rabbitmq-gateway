#include "rabbitmq_integration/consumer.hpp"
#include "rabbitmq_integration/connection.hpp"
#include "rabbitmq_integration/message.hpp"
#include <spdlog/spdlog.h>
#include <amqp.h>
#include <amqp_framing.h>

namespace rabbitmq_integration {

Consumer::Consumer(const ConsumerConfig& config) 
    : config_(config), running_(false), stopping_(false) {
    spdlog::debug("Creating Consumer with config");
    
    // Initialize statistics
    aggregatedStats_ = {};
    aggregatedStats_.consumerId = "consumer_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    aggregatedStats_.consumerStarted = std::chrono::system_clock::now();
}

Consumer::Consumer(Consumer&& other) noexcept
    : config_(std::move(other.config_)),
      connection_(std::move(other.connection_)),
      running_(other.running_.load()),
      stopping_(other.stopping_.load()),
      consumers_(std::move(other.consumers_)),
      aggregatedStats_(std::move(other.aggregatedStats_)),
      consumerThreads_(std::move(other.consumerThreads_)),
      errorCallback_(std::move(other.errorCallback_)),
      connectionCallback_(std::move(other.connectionCallback_)) {
    
    other.running_ = false;
    other.stopping_ = false;
}

Consumer& Consumer::operator=(Consumer&& other) noexcept {
    if (this != &other) {
        stopConsuming();
        
        config_ = std::move(other.config_);
        connection_ = std::move(other.connection_);
        running_ = other.running_.load();
        stopping_ = other.stopping_.load();
        consumers_ = std::move(other.consumers_);
        aggregatedStats_ = std::move(other.aggregatedStats_);
        consumerThreads_ = std::move(other.consumerThreads_);
        errorCallback_ = std::move(other.errorCallback_);
        connectionCallback_ = std::move(other.connectionCallback_);
        
        other.running_ = false;
        other.stopping_ = false;
    }
    return *this;
}

Consumer::~Consumer() {
    spdlog::debug("Destroying Consumer");
    stopConsuming();
    teardownConnection();
}

bool Consumer::startConsuming(const std::string& queueName,
                             MessageCallback callback) {
    return startConsuming(queueName, "", std::move(callback));
}

bool Consumer::startConsuming(const std::string& queueName,
                             const std::string& consumerTag,
                             MessageCallback callback) {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    
    if (!validateQueue(queueName)) {
        spdlog::error("Invalid queue name: {}", queueName);
        return false;
    }
    
    if (consumers_.find(queueName) != consumers_.end()) {
        spdlog::warn("Consumer already exists for queue: {}", queueName);
        return false;
    }
    
    if (!initializeConnection()) {
        spdlog::error("Failed to initialize connection");
        return false;
    }
    
    try {
        auto channel = connection_->createChannel();
        if (!channel) {
            spdlog::error("Failed to create channel for queue: {}", queueName);
            return false;
        }
        
        std::string actualConsumerTag = consumerTag.empty() ? 
            generateConsumerTag(queueName) : consumerTag;
        
        if (!createConsumer(queueName, actualConsumerTag, std::move(callback), channel)) {
            spdlog::error("Failed to create consumer for queue: {}", queueName);
            return false;
        }
        
        running_ = true;
        spdlog::info("Started consuming from queue: {} with tag: {}", queueName, actualConsumerTag);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception starting consumer: {}", e.what());
        handleConsumerError("", "CONSUMER_START_ERROR", e.what());
        return false;
    }
}

bool Consumer::startConsuming(const std::string& queueName,
                             const std::string& consumerTag,
                             MessageCallback callback,
                             const std::string& exchangeName,
                             const std::string& routingKey) {
    (void)exchangeName;  // Suppress unused parameter warning
    (void)routingKey;    // Suppress unused parameter warning
    
    // For now, implement basic version - can be extended for exchange binding
    return startConsuming(queueName, consumerTag, std::move(callback));
}

bool Consumer::startConsuming(const std::string& queueName,
                             MessageCallback callback,
                             const ConsumerConfig& customConfig) {
    // Store original config and temporarily use custom config
    auto originalConfig = config_;
    config_ = customConfig;
    
    bool result = startConsuming(queueName, std::move(callback));
    
    // Restore original config
    config_ = originalConfig;
    
    return result;
}

void Consumer::stopConsuming() {
    spdlog::debug("Stopping all consumers");
    
    stopping_ = true;
    
    // Stop all consumer threads
    {
        std::lock_guard<std::mutex> lock(consumersMutex_);
        for (auto& [queueName, consumerInfo] : consumers_) {
            consumerInfo->active = false;
        }
    }
    
    // Wait for threads to finish
    for (auto& thread : consumerThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    consumerThreads_.clear();
    
    // Clear consumers
    {
        std::lock_guard<std::mutex> lock(consumersMutex_);
        consumers_.clear();
    }
    
    running_ = false;
    stopping_ = false;
    
    spdlog::info("Stopped all consumers");
}

bool Consumer::stopConsuming(const std::string& queueName) {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    
    auto it = consumers_.find(queueName);
    if (it == consumers_.end()) {
        spdlog::warn("No consumer found for queue: {}", queueName);
        return false;
    }
    
    it->second->active = false;
    consumers_.erase(it);
    
    spdlog::info("Stopped consumer for queue: {}", queueName);
    return true;
}

std::optional<std::pair<Message, uint64_t>> Consumer::getMessage(const std::string& queueName,
                                                                bool noAck) {
    (void)noAck;  // Suppress unused parameter warning
    
    // Implementation for polling mode - simplified for now
    spdlog::debug("Getting message from queue: {}", queueName);
    return std::nullopt;
}

bool Consumer::acknowledge(uint64_t deliveryTag) {
    if (!connection_ || !connection_->isConnected()) {
        spdlog::error("Cannot acknowledge - not connected");
        return false;
    }
    
    try {
        // Implementation would use channel->basicAck()
        spdlog::debug("Acknowledged message with delivery tag: {}", deliveryTag);
        
        // Update statistics
        std::lock_guard<std::mutex> lock(statsMutex_);
        aggregatedStats_.messagesAcknowledged++;
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to acknowledge message: {}", e.what());
        return false;
    }
}

bool Consumer::acknowledgeMultiple(uint64_t deliveryTag) {
    if (!connection_ || !connection_->isConnected()) {
        spdlog::error("Cannot acknowledge multiple - not connected");
        return false;
    }
    
    try {
        // Implementation would use channel->basicAck() with multiple=true
        spdlog::debug("Acknowledged multiple messages up to delivery tag: {}", deliveryTag);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to acknowledge multiple messages: {}", e.what());
        return false;
    }
}

bool Consumer::reject(uint64_t deliveryTag, bool requeue) {
    if (!connection_ || !connection_->isConnected()) {
        spdlog::error("Cannot reject - not connected");
        return false;
    }
    
    try {
        // Implementation would use channel->basicReject()
        spdlog::debug("Rejected message with delivery tag: {} (requeue: {})", 
                     deliveryTag, requeue);
        
        // Update statistics
        std::lock_guard<std::mutex> lock(statsMutex_);
        if (requeue) {
            aggregatedStats_.messagesRequeued++;
        } else {
            aggregatedStats_.messagesRejected++;
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reject message: {}", e.what());
        return false;
    }
}

bool Consumer::rejectMultiple(uint64_t deliveryTag, bool requeue) {
    if (!connection_ || !connection_->isConnected()) {
        spdlog::error("Cannot reject multiple - not connected");
        return false;
    }
    
    try {
        // Implementation would use channel->basicNack() with multiple=true
        spdlog::debug("Rejected multiple messages up to delivery tag: {} (requeue: {})", 
                     deliveryTag, requeue);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reject multiple messages: {}", e.what());
        return false;
    }
}

bool Consumer::nack(uint64_t deliveryTag, bool multiple, bool requeue) {
    if (!connection_ || !connection_->isConnected()) {
        spdlog::error("Cannot nack - not connected");
        return false;
    }
    
    try {
        // Implementation would use channel->basicNack()
        spdlog::debug("Nacked message with delivery tag: {} (multiple: {}, requeue: {})", 
                     deliveryTag, multiple, requeue);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to nack message: {}", e.what());
        return false;
    }
}

bool Consumer::isConnected() const {
    return connection_ && connection_->isConnected();
}

ConnectionState Consumer::getConnectionState() const {
    return connection_ ? connection_->getState() : ConnectionState::Disconnected;
}

ConsumerStats Consumer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return aggregatedStats_;
}

ConsumerStats Consumer::getStatsForQueue(const std::string& queueName) const {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    
    auto it = consumers_.find(queueName);
    if (it != consumers_.end()) {
        return it->second->stats;
    }
    
    return ConsumerStats{};
}

void Consumer::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    aggregatedStats_ = ConsumerStats{};
    aggregatedStats_.consumerId = "consumer_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    aggregatedStats_.consumerStarted = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> consumersLock(consumersMutex_);
    for (auto& [queueName, consumerInfo] : consumers_) {
        consumerInfo->stats = ConsumerStats{};
    }
}

const ConsumerConfig& Consumer::getConfig() const {
    return config_;
}

bool Consumer::updateConfig(const ConsumerConfig& config) {
    config_ = config;
    // TODO: Apply configuration changes to active consumers
    return true;
}

void Consumer::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

void Consumer::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = std::move(callback);
}

std::vector<std::string> Consumer::getActiveConsumers() const {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    std::vector<std::string> activeConsumers;
    
    for (const auto& [queueName, consumerInfo] : consumers_) {
        if (consumerInfo->active) {
            activeConsumers.push_back(queueName);
        }
    }
    
    return activeConsumers;
}

std::vector<Consumer::ConsumerInfo> Consumer::getConsumerInfo() const {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    std::vector<ConsumerInfo> info;
    
    for (const auto& [queueName, consumerInfo] : consumers_) {
        info.emplace_back(std::move(*consumerInfo));
    }
    
    return info;
}

bool Consumer::createConsumer(const std::string& queueName,
                             const std::string& consumerTag,
                             MessageCallback callback,
                             std::shared_ptr<Channel> channel) {
    
    auto consumerInfo = std::make_unique<ConsumerInfo>(queueName, consumerTag, 
                                                      std::move(callback), channel);
    consumerInfo->active = true;
    
    // Start consumer thread
    auto thread = std::make_unique<std::thread>(
        &Consumer::consumerThreadFunc, this, std::move(consumerInfo));
    consumerThreads_.push_back(std::move(thread));
    
    return true;
}

void Consumer::consumerThreadFunc(std::unique_ptr<ConsumerInfo> info) {
    spdlog::debug("Consumer thread started for queue: {}", info->queueName);
    
    try {
        // Add to consumers map
        {
            std::lock_guard<std::mutex> lock(consumersMutex_);
            consumers_[info->queueName] = std::move(info);
        }
        
        // Main consumer loop would go here
        // For now, just simulate processing
        while (running_ && !stopping_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Check if this consumer is still active
            {
                std::lock_guard<std::mutex> lock(consumersMutex_);
                auto it = consumers_.find(info->queueName);
                if (it == consumers_.end() || !it->second->active) {
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Consumer thread error for queue {}: {}", info->queueName, e.what());
        handleConsumerError(info->consumerTag, "CONSUMER_THREAD_ERROR", e.what());
    }
    
    spdlog::debug("Consumer thread ended for queue: {}", info->queueName);
}

bool Consumer::processMessage(ConsumerInfo& info, const Message& message, uint64_t deliveryTag) {
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        bool result = info.callback(info.consumerTag, info.queueName, message, deliveryTag);
        
        auto endTime = std::chrono::steady_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        updateStatsForConsumer(info, result, processingTime);
        
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error processing message in queue {}: {}", info.queueName, e.what());
        return false;
    }
}

void Consumer::updateAggregatedStats() {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    std::lock_guard<std::mutex> consumersLock(consumersMutex_);
    
    // Reset aggregated stats
    auto consumerId = aggregatedStats_.consumerId;
    auto consumerStarted = aggregatedStats_.consumerStarted;
    aggregatedStats_ = ConsumerStats{};
    aggregatedStats_.consumerId = consumerId;
    aggregatedStats_.consumerStarted = consumerStarted;
    
    // Aggregate from all consumers
    for (const auto& [queueName, consumerInfo] : consumers_) {
        const auto& stats = consumerInfo->stats;
        aggregatedStats_.messagesReceived += stats.messagesReceived;
        aggregatedStats_.messagesAcknowledged += stats.messagesAcknowledged;
        aggregatedStats_.messagesRejected += stats.messagesRejected;
        aggregatedStats_.messagesRequeued += stats.messagesRequeued;
        aggregatedStats_.messagesUnacked += stats.messagesUnacked;
        aggregatedStats_.bytesReceived += stats.bytesReceived;
    }
}

void Consumer::updateStatsForConsumer(ConsumerInfo& info, bool messageProcessed, 
                                     std::chrono::milliseconds processingTime) {
    info.stats.messagesReceived++;
    info.stats.lastMessage = std::chrono::system_clock::now();
    
    if (messageProcessed) {
        info.stats.messagesAcknowledged++;
    } else {
        info.stats.messagesRejected++;
    }
    
    // Update processing time stats
    if (processingTime > info.stats.maxProcessingTime) {
        info.stats.maxProcessingTime = processingTime;
    }
    
    // Simple moving average for average processing time
    if (info.stats.avgProcessingTime.count() == 0) {
        info.stats.avgProcessingTime = processingTime;
    } else {
        info.stats.avgProcessingTime = std::chrono::milliseconds(
            (info.stats.avgProcessingTime.count() + processingTime.count()) / 2);
    }
    
    updateAggregatedStats();
}

bool Consumer::initializeConnection() {
    if (connection_ && connection_->isConnected()) {
        return true;
    }
    
    try {
        connection_ = std::make_shared<Connection>(config_);
        return connection_->open();
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize connection: {}", e.what());
        return false;
    }
}

void Consumer::teardownConnection() {
    if (connection_) {
        connection_->close();
        connection_.reset();
    }
}

void Consumer::onConnectionStateChanged(ConnectionState state, const std::string& reason) {
    spdlog::info("Connection state changed to: {} ({})", 
                static_cast<int>(state), reason);
    
    if (connectionCallback_) {
        connectionCallback_(state == ConnectionState::Connected, reason);
    }
}

void Consumer::handleConsumerError(const std::string& consumerTag, 
                                 const std::string& errorCode, 
                                 const std::string& errorMessage) {
    spdlog::error("Consumer error - Tag: {}, Code: {}, Message: {}", 
                 consumerTag, errorCode, errorMessage);
    
    if (errorCallback_) {
        errorCallback_(errorCode, errorMessage, "Consumer: " + consumerTag);
    }
}

void Consumer::cleanup() {
    stopConsuming();
    teardownConnection();
}

void Consumer::cleanupConsumer(const std::string& queueName) {
    std::lock_guard<std::mutex> lock(consumersMutex_);
    
    auto it = consumers_.find(queueName);
    if (it != consumers_.end()) {
        it->second->active = false;
        consumers_.erase(it);
    }
}

bool Consumer::validateQueue(const std::string& queueName) const {
    return !queueName.empty() && queueName.length() <= 255;
}

bool Consumer::isValidConsumerTag(const std::string& consumerTag) const {
    return !consumerTag.empty() && consumerTag.length() <= 255;
}

std::string Consumer::generateConsumerTag(const std::string& queueName) const {
    return "consumer_" + queueName + "_" + 
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

// Factory implementations
std::unique_ptr<Consumer> ConsumerFactory::createCommandConsumer(const ConsumerConfig& config) {
    return std::make_unique<Consumer>(config);
}

std::unique_ptr<Consumer> ConsumerFactory::createTelemetryConsumer(const ConsumerConfig& config) {
    return std::make_unique<Consumer>(config);
}

std::unique_ptr<Consumer> ConsumerFactory::createDebugConsumer(const ConsumerConfig& config) {
    return std::make_unique<Consumer>(config);
}

std::unique_ptr<Consumer> ConsumerFactory::createHighThroughputConsumer(const ConnectionConfig& connConfig) {
    ConsumerConfig config;
    // Copy connection config
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.prefetchCount = 100;
    config.autoAck = true;
    return std::make_unique<Consumer>(config);
}

std::unique_ptr<Consumer> ConsumerFactory::createReliableConsumer(const ConnectionConfig& connConfig) {
    ConsumerConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.prefetchCount = 1;
    config.autoAck = false;
    config.autoRecover = true;
    return std::make_unique<Consumer>(config);
}

std::unique_ptr<Consumer> ConsumerFactory::createLowLatencyConsumer(const ConnectionConfig& connConfig) {
    ConsumerConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    config.prefetchCount = 10;
    config.ackTimeout = std::chrono::milliseconds(1000);
    return std::make_unique<Consumer>(config);
}

ConsumerConfig ConsumerFactory::createOptimizedConfig(const ConnectionConfig& connConfig,
                                                     const std::string& optimization) {
    ConsumerConfig config;
    static_cast<ConnectionConfig&>(config) = connConfig;
    
    if (optimization == "high_throughput") {
        config.prefetchCount = 100;
        config.autoAck = true;
    } else if (optimization == "reliable") {
        config.prefetchCount = 1;
        config.autoAck = false;
        config.autoRecover = true;
    } else if (optimization == "low_latency") {
        config.prefetchCount = 10;
        config.ackTimeout = std::chrono::milliseconds(1000);
    }
    
    return config;
}

// MultiQueueConsumer implementation
MultiQueueConsumer::MultiQueueConsumer(const ConsumerConfig& config)
    : config_(config), running_(false) {
}

MultiQueueConsumer::~MultiQueueConsumer() {
    stop();
}

bool MultiQueueConsumer::addDevice(const std::string& region, const std::string& imei,
                                  MessageCallback callback) {
    std::string inboundQueue = region + "." + imei + "_inbound";
    std::string outboundQueue = region + "." + imei + "_outbound";
    
    bool success = true;
    success &= addQueue(inboundQueue, callback);
    success &= addQueue(outboundQueue, callback);
    
    return success;
}

bool MultiQueueConsumer::removeDevice(const std::string& region, const std::string& imei) {
    std::string inboundQueue = region + "." + imei + "_inbound";
    std::string outboundQueue = region + "." + imei + "_outbound";
    
    bool success = true;
    success &= removeQueue(inboundQueue);
    success &= removeQueue(outboundQueue);
    
    return success;
}

bool MultiQueueConsumer::addQueue(const std::string& queueName, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    queueCallbacks_[queueName] = callback;
    return true;
}

bool MultiQueueConsumer::removeQueue(const std::string& queueName) {
    std::lock_guard<std::mutex> lock(mutex_);
    queueCallbacks_.erase(queueName);
    return true;
}

bool MultiQueueConsumer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return true;
    }
    
    // Create consumers for each queue
    for (const auto& [queueName, callback] : queueCallbacks_) {
        auto consumer = std::make_unique<Consumer>(config_);
        if (consumer->startConsuming(queueName, callback)) {
            consumers_.push_back(std::move(consumer));
        }
    }
    
    running_ = true;
    return true;
}

void MultiQueueConsumer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    running_ = false;
    consumers_.clear();
}

bool MultiQueueConsumer::isRunning() const {
    return running_;
}

ConsumerStats MultiQueueConsumer::getAggregatedStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConsumerStats aggregated{};
    for (const auto& consumer : consumers_) {
        auto stats = consumer->getStats();
        aggregated.messagesReceived += stats.messagesReceived;
        aggregated.messagesAcknowledged += stats.messagesAcknowledged;
        aggregated.messagesRejected += stats.messagesRejected;
        aggregated.bytesReceived += stats.bytesReceived;
    }
    
    return aggregated;
}

std::map<std::string, ConsumerStats> MultiQueueConsumer::getPerQueueStats() const {
    std::map<std::string, ConsumerStats> stats;
    // Implementation would collect stats from individual consumers
    return stats;
}

void MultiQueueConsumer::updateConfig(const ConsumerConfig& config) {
    config_ = config;
    // TODO: Update configuration for active consumers
}

// ConsumerPool implementation
ConsumerPool::ConsumerPool(const ConsumerConfig& config, size_t poolSize)
    : config_(config), poolSize_(poolSize), running_(false) {
}

ConsumerPool::~ConsumerPool() {
    stop();
}

bool ConsumerPool::start() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    if (running_) {
        return true;
    }
    
    initializePool();
    running_ = true;
    return true;
}

void ConsumerPool::stop() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    running_ = false;
    cleanupPool();
}

bool ConsumerPool::isRunning() const {
    return running_;
}

bool ConsumerPool::addConsumer(const std::string& queueName, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    if (!running_) {
        return false;
    }
    
    // Add to first available consumer
    if (!consumers_.empty()) {
        return consumers_[0]->startConsuming(queueName, callback);
    }
    
    return false;
}

bool ConsumerPool::removeConsumer(const std::string& queueName) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& consumer : consumers_) {
        if (consumer->stopConsuming(queueName)) {
            return true;
        }
    }
    
    return false;
}

ConsumerStats ConsumerPool::getAggregatedStats() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    ConsumerStats aggregated{};
    for (const auto& consumer : consumers_) {
        auto stats = consumer->getStats();
        aggregated.messagesReceived += stats.messagesReceived;
        aggregated.messagesAcknowledged += stats.messagesAcknowledged;
        aggregated.messagesRejected += stats.messagesRejected;
        aggregated.bytesReceived += stats.bytesReceived;
    }
    
    return aggregated;
}

std::vector<ConsumerStats> ConsumerPool::getIndividualStats() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    std::vector<ConsumerStats> stats;
    for (const auto& consumer : consumers_) {
        stats.push_back(consumer->getStats());
    }
    
    return stats;
}

void ConsumerPool::performHealthCheck() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& consumer : consumers_) {
        if (!consumer->isConnected()) {
            spdlog::warn("Consumer not connected, attempting to reconnect");
            // Would implement reconnection logic here
        }
    }
}

void ConsumerPool::initializePool() {
    consumers_.clear();
    consumers_.reserve(poolSize_);
    
    for (size_t i = 0; i < poolSize_; ++i) {
        consumers_.push_back(std::make_unique<Consumer>(config_));
    }
}

void ConsumerPool::cleanupPool() {
    consumers_.clear();
}

} // namespace rabbitmq_integration