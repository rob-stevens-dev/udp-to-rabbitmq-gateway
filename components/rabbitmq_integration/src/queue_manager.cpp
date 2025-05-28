#include "rabbitmq_integration/queue_manager.hpp"
#include "rabbitmq_integration/connection.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <cstddef>
#include <exception>
#include <spdlog/spdlog.h>

namespace rabbitmq_integration {

QueueManager::QueueManager(const ConnectionConfig& config)
    : config_(config), ownsConnection_(true) {
    // Constructor implementation will be added back
}

QueueManager::QueueManager(std::shared_ptr<Connection> connection)
    : connection_(connection), ownsConnection_(false) {
    // Constructor implementation will be added back
}

QueueManager::~QueueManager() {
    teardownConnection();
}

QueueManager::QueueManager(QueueManager&& other) noexcept
    : config_(std::move(other.config_)),
      connection_(std::move(other.connection_)),
      channel_(std::move(other.channel_)),
      ownsConnection_(other.ownsConnection_),
      lastError_(std::move(other.lastError_)),           // Line 114 in header
      errorCallback_(std::move(other.errorCallback_)),   // Line 115 in header
      queueTemplates_(std::move(other.queueTemplates_)), // Queue templates section
      queueExistsCache_(std::move(other.queueExistsCache_)),      // Cache section
      exchangeExistsCache_(std::move(other.exchangeExistsCache_)), // Cache section
      lastCacheUpdate_(other.lastCacheUpdate_),          // Line 124 in header
      cacheExpiry_(other.cacheExpiry_) {                 // Line 125 in header
    other.ownsConnection_ = false;
    other.connection_.reset();
    other.channel_.reset();
}

QueueManager& QueueManager::operator=(QueueManager&& other) noexcept {
    if (this != &other) {
        teardownConnection();
        
        config_ = std::move(other.config_);
        connection_ = std::move(other.connection_);
        channel_ = std::move(other.channel_);
        ownsConnection_ = other.ownsConnection_;
        queueExistsCache_ = std::move(other.queueExistsCache_);
        exchangeExistsCache_ = std::move(other.exchangeExistsCache_);
        lastCacheUpdate_ = other.lastCacheUpdate_;
        cacheExpiry_ = other.cacheExpiry_;
        lastError_ = std::move(other.lastError_);
        errorCallback_ = std::move(other.errorCallback_);
        queueTemplates_ = std::move(other.queueTemplates_);
        
        other.ownsConnection_ = false;
        other.connection_.reset();
        other.channel_.reset();
    }
    return *this;
}

// Rest of the implementation will be gradually added back as we fix issues

} // namespace rabbitmq_integration