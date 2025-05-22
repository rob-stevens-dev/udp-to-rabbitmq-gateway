// components/redis-deduplication/src/redis_client.cpp
#include "redis_deduplication/redis_client.hpp"
#include <spdlog/spdlog.h>
#include <stdarg.h>
#include <cstring>
#include <algorithm>

namespace redis_deduplication {

RedisClient::RedisClient(const std::string& host, int port, 
                         const std::string& password,
                         std::chrono::milliseconds connectionTimeout,
                         std::chrono::milliseconds socketTimeout)
    : host_(host), port_(port), password_(password),
      connectionTimeout_(connectionTimeout), socketTimeout_(socketTimeout),
      context_(nullptr), connectionStatus_(ConnectionStatus::DISCONNECTED),
      errorCount_(0) {
    
    spdlog::debug("RedisClient created for {}:{}", host_, port_);
}

RedisClient::~RedisClient() {
    disconnect();
}

RedisClient::RedisClient(RedisClient&& other) noexcept
    : host_(std::move(other.host_)), port_(other.port_),
      password_(std::move(other.password_)),
      connectionTimeout_(other.connectionTimeout_),
      socketTimeout_(other.socketTimeout_),
      context_(other.context_),
      connectionStatus_(other.connectionStatus_.load()),
      lastConnectTime_(other.lastConnectTime_),
      lastErrorTime_(other.lastErrorTime_),
      errorCount_(other.errorCount_.load()),
      lastError_(std::move(other.lastError_)) {
    
    other.context_ = nullptr;
    other.connectionStatus_ = ConnectionStatus::DISCONNECTED;
    other.errorCount_ = 0;
}

RedisClient& RedisClient::operator=(RedisClient&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        disconnect();
        
        // Move data
        host_ = std::move(other.host_);
        port_ = other.port_;
        password_ = std::move(other.password_);
        connectionTimeout_ = other.connectionTimeout_;
        socketTimeout_ = other.socketTimeout_;
        context_ = other.context_;
        connectionStatus_ = other.connectionStatus_.load();
        lastConnectTime_ = other.lastConnectTime_;
        lastErrorTime_ = other.lastErrorTime_;
        errorCount_ = other.errorCount_.load();
        lastError_ = std::move(other.lastError_);
        
        // Reset other object
        other.context_ = nullptr;
        other.connectionStatus_ = ConnectionStatus::DISCONNECTED;
        other.errorCount_ = 0;
    }
    return *this;
}

bool RedisClient::connect() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    // Cleanup existing connection
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    
    connectionStatus_ = ConnectionStatus::CONNECTING;
    
    // Create connection with timeout
    struct timeval timeout;
    timeout.tv_sec = connectionTimeout_.count() / 1000;
    timeout.tv_usec = (connectionTimeout_.count() % 1000) * 1000;
    
    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout);
    
    if (!context_) {
        handleConnectionError("Failed to allocate Redis context");
        return false;
    }
    
    if (context_->err) {
        std::string error = "Connection failed: " + std::string(context_->errstr);
        handleConnectionError(error);
        redisFree(context_);
        context_ = nullptr;
        return false;
    }
    
    // Set socket timeout
    struct timeval sockTimeout;
    sockTimeout.tv_sec = socketTimeout_.count() / 1000;
    sockTimeout.tv_usec = (socketTimeout_.count() % 1000) * 1000;
    
    if (redisSetTimeout(context_, sockTimeout) != REDIS_OK) {
        spdlog::warn("Failed to set socket timeout for Redis connection");
    }
    
    // Authenticate if password provided
    if (!authenticate()) {
        redisFree(context_);
        context_ = nullptr;
        return false;
    }
    
    // Test connection with PING
    if (!ping()) {
        redisFree(context_);
        context_ = nullptr;
        return false;
    }
    
    connectionStatus_ = ConnectionStatus::CONNECTED;
    lastConnectTime_ = std::chrono::system_clock::now();
    
    spdlog::info("Successfully connected to Redis at {}:{}", host_, port_);
    return true;
}

void RedisClient::disconnect() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    
    connectionStatus_ = ConnectionStatus::DISCONNECTED;
    spdlog::debug("Disconnected from Redis at {}:{}", host_, port_);
}

ConnectionStatus RedisClient::getConnectionStatus() const {
    return connectionStatus_.load();
}

bool RedisClient::setNX(const std::string& key, std::chrono::milliseconds ttlMs) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        return false;
    }
    
    // Use SET key value NX PX ttl_milliseconds
    RedisReply reply(static_cast<redisReply*>(
        redisCommand(context_, "SET %s 1 NX PX %lld", 
                     key.c_str(), static_cast<long long>(ttlMs.count()))));
    
    if (!reply) {
        handleConnectionError("SET NX command failed");
        return false;
    }
    
    if (isErrorReply(reply.get())) {
        setLastError("Redis error: " + std::string(reply->str ? reply->str : "Unknown error"));
        return false;
    }
    
    // If reply is OK, key was set (not duplicate)
    // If reply is null, key already existed (duplicate)
    return reply->type == REDIS_REPLY_STATUS && 
           reply->str && strcmp(reply->str, "OK") == 0;
}

std::vector<bool> RedisClient::batchSetNX(const std::vector<std::string>& keys, 
                                          std::chrono::milliseconds ttlMs) {
    std::vector<bool> results;
    results.reserve(keys.size());
    
    if (keys.empty()) {
        return results;
    }
    
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        // Return all false (conservative - treat as duplicates)
        results.assign(keys.size(), false);
        return results;
    }
    
    // Use pipelining for batch operations
    for (const auto& key : keys) {
        redisAppendCommand(context_, "SET %s 1 NX PX %lld", 
                          key.c_str(), static_cast<long long>(ttlMs.count()));
    }
    
    // Collect results
    for (size_t i = 0; i < keys.size(); ++i) {
        redisReply* reply = nullptr;
        if (redisGetReply(context_, reinterpret_cast<void**>(&reply)) != REDIS_OK) {
            handleConnectionError("Pipeline command failed");
            results.push_back(false); // Conservative - treat as duplicate
            continue;
        }
        
        RedisReply replyWrapper(reply);
        
        if (!replyWrapper || isErrorReply(replyWrapper.get())) {
            spdlog::warn("Redis SET NX failed for key: {}", keys[i]);
            results.push_back(false); // Conservative - treat as duplicate
            continue;
        }
        
        // Check if SET was successful (key was new)
        bool wasNew = replyWrapper->type == REDIS_REPLY_STATUS && 
                      replyWrapper->str && strcmp(replyWrapper->str, "OK") == 0;
        results.push_back(wasNew);
    }
    
    return results;
}

bool RedisClient::deleteKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        return false;
    }
    
    RedisReply reply(static_cast<redisReply*>(
        redisCommand(context_, "DEL %s", key.c_str())));
    
    if (!reply) {
        handleConnectionError("DEL command failed");
        return false;
    }
    
    if (isErrorReply(reply.get())) {
        setLastError("Redis error: " + std::string(reply->str ? reply->str : "Unknown error"));
        return false;
    }
    
    // Return true if at least one key was deleted
    return reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
}

int RedisClient::deleteKeys(const std::vector<std::string>& keys) {
    if (keys.empty()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        return 0;
    }
    
    // Build DEL command with all keys
    std::string command = "DEL";
    for (const auto& key : keys) {
        command += " " + key;
    }
    
    RedisReply reply(static_cast<redisReply*>(
        redisCommand(context_, command.c_str())));
    
    if (!reply) {
        handleConnectionError("DEL command failed");
        return 0;
    }
    
    if (isErrorReply(reply.get())) {
        setLastError("Redis error: " + std::string(reply->str ? reply->str : "Unknown error"));
        return 0;
    }
    
    return reply->type == REDIS_REPLY_INTEGER ? 
           static_cast<int>(reply->integer) : 0;
}

RedisReply RedisClient::executeCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        return RedisReply(nullptr);
    }
    
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, command.c_str()));
    
    if (!reply) {
        handleConnectionError("Command execution failed: " + command);
        return RedisReply(nullptr);
    }
    
    return RedisReply(reply);
}

RedisReply RedisClient::executeCommandf(const char* format, ...) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        handleConnectionError("Not connected to Redis");
        return RedisReply(nullptr);
    }
    
    va_list args;
    va_start(args, format);
    redisReply* reply = static_cast<redisReply*>(
        redisvCommand(context_, format, args));
    va_end(args);
    
    if (!reply) {
        handleConnectionError("Formatted command execution failed");
        return RedisReply(nullptr);
    }
    
    return RedisReply(reply);
}

bool RedisClient::ping() {
    // Note: This method may be called with contextMutex_ already held
    
    if (!context_ || connectionStatus_ != ConnectionStatus::CONNECTED) {
        return false;
    }
    
    RedisReply reply(static_cast<redisReply*>(
        redisCommand(context_, "PING")));
    
    if (!reply) {
        return false;
    }
    
    return reply->type == REDIS_REPLY_STATUS && 
           reply->str && strcmp(reply->str, "PONG") == 0;
}

std::string RedisClient::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void RedisClient::resetErrorCounters() {
    errorCount_ = 0;
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

bool RedisClient::authenticate() {
    if (password_.empty()) {
        return true; // No authentication needed
    }
    
    RedisReply reply(static_cast<redisReply*>(
        redisCommand(context_, "AUTH %s", password_.c_str())));
    
    if (!reply) {
        handleConnectionError("AUTH command failed");
        return false;
    }
    
    if (isErrorReply(reply.get())) {
        handleConnectionError("Authentication failed: " + 
                             std::string(reply->str ? reply->str : "Invalid password"));
        return false;
    }
    
    return reply->type == REDIS_REPLY_STATUS && 
           reply->str && strcmp(reply->str, "OK") == 0;
}

void RedisClient::handleConnectionError(const std::string& operation) {
    connectionStatus_ = ConnectionStatus::FAILED;
    lastErrorTime_ = std::chrono::system_clock::now();
    errorCount_++;
    
    std::string fullError = operation;
    if (context_ && context_->err) {
        fullError += ": " + std::string(context_->errstr);
    }
    
    setLastError(fullError);
    spdlog::error("Redis connection error [{}:{}]: {}", host_, port_, fullError);
}

bool RedisClient::isErrorReply(redisReply* reply) const {
    return reply && reply->type == REDIS_REPLY_ERROR;
}

void RedisClient::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
}

} // namespace redis_deduplication