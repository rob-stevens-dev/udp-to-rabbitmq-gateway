// include/rabbitmq-integration/local_queue.hpp
#pragma once

//#include "types.hpp"
//#include "message.hpp"
#include <queue>
#include <mutex>
#include <optional>
#include <tuple>

namespace rabbitmq_integration {

// Forward declaration
class Message;

// Local queue for storing messages during RabbitMQ outages
class LocalQueue {
public:
    using QueueItem = std::tuple<std::string, std::string, Message>; // exchange, routingKey, message
    
    // Constructor
    explicit LocalQueue(size_t maxSize);
    
    // Destructor
    ~LocalQueue() = default;
    
    // Non-copyable, but movable
    LocalQueue(const LocalQueue&) = delete;
    LocalQueue& operator=(const LocalQueue&) = delete;
    LocalQueue(LocalQueue&&) = default;
    LocalQueue& operator=(LocalQueue&&) = default;
    
    // Queue operations
    bool enqueue(const std::string& exchange, const std::string& routingKey, const Message& message);
    std::optional<QueueItem> dequeue();
    
    // Queue state
    bool isEmpty() const;
    bool isFull() const;
    size_t size() const;
    size_t capacity() const;
    
    // Queue management
    void clear();
    void setMaxSize(size_t maxSize);
    
    // Statistics
    struct Stats {
        size_t totalEnqueued = 0;
        size_t totalDequeued = 0;
        size_t totalDropped = 0;
        size_t currentSize = 0;
        size_t maxSize = 0;
        size_t peakSize = 0;
    };
    
    Stats getStats() const;
    void resetStats();

private:
    mutable std::mutex mutex_;
    std::queue<QueueItem> queue_;
    size_t maxSize_;
    
    // Statistics
    mutable Stats stats_;
    
    void updateStats();
};

} // namespace rabbitmq_integration