// components/redis-deduplication/example/config_example.cpp
#include "redis_deduplication/types.hpp"
#include <iostream>
#include <iomanip>

using namespace redis_deduplication;

void printConfig(const std::string& name, const DeduplicationConfig& config) {
    std::cout << "\n=== " << name << " ===" << std::endl;
    std::cout << "Master Host: " << config.masterHost << ":" << config.masterPort << std::endl;
    
    if (!config.standbyHost.empty()) {
        std::cout << "Standby Host: " << config.standbyHost << ":" << config.standbyPort << std::endl;
    } else {
        std::cout << "Standby Host: Not configured" << std::endl;
    }
    
    std::cout << "Password: " << (config.password.empty() ? "Not set" : "***") << std::endl;
    std::cout << "Connection Pool Size: " << config.connectionPoolSize << std::endl;
    std::cout << "Connection Timeout: " << config.connectionTimeout.count() << "ms" << std::endl;
    std::cout << "Socket Timeout: " << config.socketTimeout.count() << "ms" << std::endl;
    std::cout << "Deduplication Window: " << config.deduplicationWindow.count() << "ms" << std::endl;
    std::cout << "Local Cache Size: " << config.localCacheSize << std::endl;
    std::cout << "Local Cache Fallback: " << (config.enableLocalCacheFallback ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Local Cache TTL: " << config.localCacheTtl.count() << "ms" << std::endl;
    std::cout << "Key Prefix: " << config.keyPrefix << std::endl;
    std::cout << "Reconnect Interval: " << config.reconnectInterval.count() << "ms" << std::endl;
    std::cout << "Max Reconnect Attempts: " << config.maxReconnectAttempts << std::endl;
    std::cout << "Backoff Multiplier: " << std::fixed << std::setprecision(1) << config.backoffMultiplier << std::endl;
    std::cout << "Max Reconnect Interval: " << config.maxReconnectInterval.count() << "ms" << std::endl;
    std::cout << "Enable Pipelining: " << (config.enablePipelining ? "Yes" : "No") << std::endl;
    std::cout << "Max Pipeline Size: " << config.maxPipelineSize << std::endl;
    std::cout << "Debug Logging: " << (config.enableDebugLogging ? "Enabled" : "Disabled") << std::endl;
}

int main() {
    std::cout << "Redis Deduplication Configuration Examples" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Default configuration
    DeduplicationConfig defaultConfig;
    printConfig("Default Configuration", defaultConfig);
    
    // Development configuration
    DeduplicationConfig devConfig;
    devConfig.masterHost = "localhost";
    devConfig.masterPort = 6379;
    devConfig.connectionPoolSize = 3;
    devConfig.connectionTimeout = std::chrono::milliseconds{5000};
    devConfig.socketTimeout = std::chrono::milliseconds{3000};
    devConfig.deduplicationWindow = std::chrono::milliseconds{300000}; // 5 minutes
    devConfig.localCacheSize = 1000;
    devConfig.enableLocalCacheFallback = true;
    devConfig.localCacheTtl = std::chrono::milliseconds{300000}; // 5 minutes
    devConfig.reconnectInterval = std::chrono::milliseconds{2000};
    devConfig.maxReconnectAttempts = 3;
    devConfig.backoffMultiplier = 1.5;
    devConfig.maxReconnectInterval = std::chrono::milliseconds{30000}; // 30 seconds
    devConfig.enablePipelining = false; // Easier debugging
    devConfig.maxPipelineSize = 10;
    devConfig.enableDebugLogging = true;
    printConfig("Development Configuration", devConfig);
    
    // Production configuration
    DeduplicationConfig prodConfig;
    prodConfig.masterHost = "redis-master.example.com";
    prodConfig.masterPort = 6379;
    prodConfig.standbyHost = "redis-standby.example.com";
    prodConfig.standbyPort = 6379;
    prodConfig.password = "secure-password";
    prodConfig.connectionPoolSize = 10;
    prodConfig.connectionTimeout = std::chrono::milliseconds{3000};
    prodConfig.socketTimeout = std::chrono::milliseconds{2000};
    prodConfig.deduplicationWindow = std::chrono::milliseconds{1800000}; // 30 minutes
    prodConfig.localCacheSize = 50000;
    prodConfig.enableLocalCacheFallback = true;
    prodConfig.localCacheTtl = std::chrono::milliseconds{1800000}; // 30 minutes
    prodConfig.reconnectInterval = std::chrono::milliseconds{1000};
    prodConfig.maxReconnectAttempts = 5;
    prodConfig.backoffMultiplier = 2.0;
    prodConfig.maxReconnectInterval = std::chrono::milliseconds{60000}; // 1 minute
    prodConfig.enablePipelining = true;
    prodConfig.maxPipelineSize = 200;
    prodConfig.enableDebugLogging = false;
    printConfig("Production Configuration", prodConfig);
    
    // Custom configuration example
    std::cout << "\n=== Custom Configuration Example ===" << std::endl;
    DeduplicationConfig customConfig;
    customConfig.masterHost = "redis-cluster.example.com";
    customConfig.masterPort = 6380;
    customConfig.standbyHost = "redis-cluster-backup.example.com";
    customConfig.standbyPort = 6380;
    customConfig.password = "custom-password";
    customConfig.connectionPoolSize = 15;
    customConfig.deduplicationWindow = std::chrono::hours(2); // 2 hours
    customConfig.localCacheSize = 25000;
    customConfig.enableLocalCacheFallback = true;
    customConfig.localCacheTtl = std::chrono::hours(2); // Match dedup window
    customConfig.keyPrefix = "myapp-dedup";
    customConfig.reconnectInterval = std::chrono::seconds(3);
    customConfig.maxReconnectAttempts = 8;
    customConfig.backoffMultiplier = 1.8;
    customConfig.maxReconnectInterval = std::chrono::minutes(2);
    customConfig.enablePipelining = true;
    customConfig.maxPipelineSize = 150;
    customConfig.enableDebugLogging = false;
    
    printConfig("Custom Configuration", customConfig);
    
    // Configuration validation examples
    std::cout << "\n=== Configuration Validation ===" << std::endl;
    
    std::cout << "Default config valid: " << (isValid(defaultConfig) ? "Yes" : "No") << std::endl;
    std::cout << "Development config valid: " << (isValid(devConfig) ? "Yes" : "No") << std::endl;
    std::cout << "Production config valid: " << (isValid(prodConfig) ? "Yes" : "No") << std::endl;
    std::cout << "Custom config valid: " << (isValid(customConfig) ? "Yes" : "No") << std::endl;
    
    // Invalid configuration examples
    DeduplicationConfig invalidConfig1;
    invalidConfig1.masterHost = ""; // Invalid empty host
    std::cout << "Invalid config (empty host): " << (isValid(invalidConfig1) ? "Yes" : "No") << std::endl;
    
    DeduplicationConfig invalidConfig2;
    invalidConfig2.masterPort = 0; // Invalid port
    std::cout << "Invalid config (zero port): " << (isValid(invalidConfig2) ? "Yes" : "No") << std::endl;
    
    DeduplicationConfig invalidConfig3;
    invalidConfig3.connectionPoolSize = 0; // Invalid pool size
    std::cout << "Invalid config (zero pool size): " << (isValid(invalidConfig3) ? "Yes" : "No") << std::endl;
    
    std::cout << "\n=== Configuration Tips ===" << std::endl;
    std::cout << "1. Match localCacheTtl with deduplicationWindow for consistency" << std::endl;
    std::cout << "2. Increase connectionPoolSize for high concurrency applications" << std::endl;
    std::cout << "3. Enable pipelining for batch operations to improve throughput" << std::endl;
    std::cout << "4. Use shorter deduplicationWindow in EU for GDPR compliance" << std::endl;
    std::cout << "5. Configure standby hosts for high availability deployments" << std::endl;
    std::cout << "6. Use region-specific key prefixes to avoid conflicts" << std::endl;
    std::cout << "7. Adjust timeouts based on network conditions" << std::endl;
    std::cout << "8. Enable debug logging only in development environments" << std::endl;
    
    return 0;
}