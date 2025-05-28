#include "rabbitmq_integration/local_queue.hpp"
#include "rabbitmq_integration/message.hpp"
#include <spdlog/spdlog.h>

namespace rabbitmq_integration {

LocalQueue::LocalQueue(size_t maxSize) 
    : maxSize_(maxSize) {
    stats_.maxSize = maxSize;
}

bool LocalQueue::enqueue(const std::string& exchange, const std::string& routingKey, const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.size() >= maxSize_) {
        stats_.totalDropped++;
        spdlog::warn("Local queue full ({} items), dropping message", maxSize_);
        return false;
    }
    
    queue_.emplace(exchange, routingKey, message);
    stats_.totalEnqueued++;
    updateStats();
    
    spdlog::debug("Enqueued message to local queue, size: {}", queue_.size());
    return true;
}

std::optional<LocalQueue::QueueItem> LocalQueue::dequeue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return std::nullopt;
    }
    
    auto item = std::move(queue_.front());
    queue_.pop();
    stats_.totalDequeued++;
    updateStats();
    
    spdlog::debug("Dequeued message from local queue, size: {}", queue_.size());
    return item;
}

bool LocalQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

bool LocalQueue::isFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() >= maxSize_;
}

size_t LocalQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

size_t LocalQueue::capacity() const {
    return maxSize_;
}

void LocalQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t clearedCount = queue_.size();
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    updateStats();
    spdlog::info("Cleared {} messages from local queue", clearedCount);
}

void LocalQueue::setMaxSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    maxSize_ = maxSize;
    stats_.maxSize = maxSize;
    
    // Remove excess items if new size is smaller
    while (queue_.size() > maxSize_) {
        queue_.pop();
        stats_.totalDropped++;
    }
    
    updateStats();
}

LocalQueue::Stats LocalQueue::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void LocalQueue::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
    stats_.maxSize = maxSize_;
    stats_.currentSize = queue_.size();
}

void LocalQueue::updateStats() {
    stats_.currentSize = queue_.size();
    if (stats_.currentSize > stats_.peakSize) {
        stats_.peakSize = stats_.currentSize;
    }
}

} // namespace rabbitmq_integration