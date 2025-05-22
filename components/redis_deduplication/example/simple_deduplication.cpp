// components/redis-deduplication/example/simple_deduplication.cpp
#include "redis_deduplication/deduplicator.hpp"
#include "redis_deduplication/types.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

using namespace redis_deduplication;

void printStats(const DeduplicationStats& stats) {
    std::cout << "\n=== Deduplication Statistics ===" << std::endl;
    std::cout << "Total Checks: " << stats.totalChecks << std::endl;
    std::cout << "Duplicates Found: " << stats.duplicatesFound << std::endl;
    std::cout << "Unique Messages: " << stats.uniqueMessages << std::endl;
    std::cout << "Redis Failures: " << stats.redisFailures << std::endl;
    std::cout << "Cache Hits: " << stats.localCacheHits << std::endl;
    std::cout << "Cache Misses: " << stats.localCacheMisses << std::endl;
    std::cout << "Average Latency: " << stats.averageCheckLatency.count() << "μs" << std::endl;
    std::cout << "Active Connections: " << stats.activeConnections << std::endl;
    std::cout << "Total Connections: " << stats.totalConnections << std::endl;
    std::cout << "================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "Redis Deduplication Example" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Parse command line arguments
    std::string redisHost = "localhost";
    int redisPort = 6379;
    std::string redisPassword;
    
    if (argc >= 2) {
        redisHost = argv[1];
    }
    if (argc >= 3) {
        redisPort = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        redisPassword = argv[3];
    }
    
    std::cout << "Connecting to Redis at " << redisHost << ":" << redisPort << std::endl;
    
    try {
        // Create configuration manually (simplified)
        DeduplicationConfig config;
        config.masterHost = redisHost;
        config.masterPort = redisPort;
        config.password = redisPassword;
        config.deduplicationWindow = std::chrono::milliseconds{60000}; // 1 minute
        config.localCacheSize = 100;
        config.connectionPoolSize = 3;
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Master Host: " << config.masterHost << ":" << config.masterPort << std::endl;
        std::cout << "  Deduplication Window: " << config.deduplicationWindow.count() << "ms" << std::endl;
        std::cout << "  Local Cache Size: " << config.localCacheSize << std::endl;
        std::cout << "  Connection Pool Size: " << config.connectionPoolSize << std::endl;
        
        // Create and initialize deduplicator
        Deduplicator deduplicator(config);
        
        if (!deduplicator.initialize()) {
            std::cerr << "Failed to initialize deduplicator" << std::endl;
            return 1;
        }
        
        std::cout << "Deduplicator initialized successfully" << std::endl;
        
        // Perform health check
        if (deduplicator.healthCheck()) {
            std::cout << "Health check passed" << std::endl;
        } else {
            std::cout << "Health check failed" << std::endl;
        }
        
        std::cout << "\n--- Single Message Tests ---" << std::endl;
        
        // Test 1: Single message deduplication
        std::string testImei = "123456789012345";
        uint32_t seqNum = 100;
        
        std::cout << "Testing message: IMEI=" << testImei << ", SeqNum=" << seqNum << std::endl;
        
        auto result1 = deduplicator.isDuplicate(testImei, seqNum);
        std::cout << "First check result: ";
        switch (result1) {
            case DeduplicationResult::UNIQUE:
                std::cout << "UNIQUE";
                break;
            case DeduplicationResult::DUPLICATE:
                std::cout << "DUPLICATE";
                break;
            case DeduplicationResult::ERROR:
                std::cout << "ERROR";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << std::endl;
        
        auto result2 = deduplicator.isDuplicate(testImei, seqNum);
        std::cout << "Second check result: ";
        switch (result2) {
            case DeduplicationResult::UNIQUE:
                std::cout << "UNIQUE";
                break;
            case DeduplicationResult::DUPLICATE:
                std::cout << "DUPLICATE";
                break;
            case DeduplicationResult::ERROR:
                std::cout << "ERROR";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << std::endl;
        
        // Test 2: Different sequence number
        auto result3 = deduplicator.isDuplicate(testImei, seqNum + 1);
        std::cout << "Different sequence result: ";
        switch (result3) {
            case DeduplicationResult::UNIQUE:
                std::cout << "UNIQUE";
                break;
            case DeduplicationResult::DUPLICATE:
                std::cout << "DUPLICATE";
                break;
            case DeduplicationResult::ERROR:
                std::cout << "ERROR";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << std::endl;
        
        std::cout << "\n--- Batch Message Tests ---" << std::endl;
        
        // Test 3: Batch deduplication
        std::vector<std::pair<std::string, uint32_t>> batchMessages = {
            {"123456789012345", 200},  // New message
            {"123456789012345", 201},  // New message
            {"123456789012345", 200},  // Duplicate of first
            {"987654321098765", 300},  // Different device
            {"987654321098765", 300},  // Duplicate of previous
            {"555555555555555", 400},  // Another device
        };
        
        std::cout << "Testing batch of " << batchMessages.size() << " messages:" << std::endl;
        
        auto batchResult = deduplicator.areDuplicates(batchMessages);
        
        for (size_t i = 0; i < batchMessages.size(); ++i) {
            std::cout << "  Message " << i + 1 << " [" << batchMessages[i].first 
                      << ":" << batchMessages[i].second << "] -> ";
            
            switch (batchResult.results[i]) {
                case DeduplicationResult::UNIQUE:
                    std::cout << "UNIQUE";
                    break;
                case DeduplicationResult::DUPLICATE:
                    std::cout << "DUPLICATE";
                    break;
                case DeduplicationResult::ERROR:
                    std::cout << "ERROR";
                    break;
                default:
                    std::cout << "UNKNOWN";
                    break;
            }
            std::cout << std::endl;
        }
        
        std::cout << "Batch operation took: " << batchResult.totalLatency.count() << "μs" << std::endl;
        std::cout << "Redis operations: " << batchResult.redisOperations << std::endl;
        std::cout << "Cache hits: " << batchResult.cacheHits << std::endl;
        std::cout << "Errors: " << batchResult.errors << std::endl;
        
        std::cout << "\n--- Connection Tests ---" << std::endl;
        
        // Test 4: Connection status
        std::cout << "Connection status: " << (deduplicator.isConnected() ? "Connected" : "Disconnected") << std::endl;
        std::cout << "Using standby: " << (deduplicator.isUsingStandby() ? "Yes" : "No") << std::endl;
        
        // Final statistics
        printStats(deduplicator.getStats());
        
        std::cout << "\n--- Shutdown Test ---" << std::endl;
        
        // Test 5: Graceful shutdown
        std::cout << "Shutting down deduplicator..." << std::endl;
        deduplicator.shutdown();
        
        std::cout << "Shutdown complete" << std::endl;
        
        std::cout << "\nExample completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}