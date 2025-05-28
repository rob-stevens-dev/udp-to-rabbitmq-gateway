#pragma once

#include "rabbitmq_integration/types.hpp"
#include "rabbitmq_integration/message.hpp"
#include <gtest/gtest.h>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace rabbitmq_integration {
namespace test {

// Test configuration helpers
class TestConfig {
public:
    static ConnectionConfig getTestConnectionConfig();
    static PublisherConfig getTestPublisherConfig();
    static ConsumerConfig getTestConsumerConfig();
    
    // Environment variable helpers
    static std::string getEnvVar(const std::string& name, const std::string& defaultValue = "");
    static bool isRabbitMQAvailable();
    static void waitForRabbitMQ(std::chrono::seconds timeout = std::chrono::seconds(30));
};

// Test message creators
class TestMessages {
public:
    static Message createTestTelemetryMessage(const std::string& imei = "123456789012345");
    static Message createTestCommandMessage(const std::string& command = "test_command");
    static Message createTestDebugMessage(const std::string& imei = "123456789012345");
    static Message createTestDiscardMessage(const std::string& imei = "123456789012345");
    static Message createLargeMessage(size_t sizeBytes);
    static Message createBinaryMessage(const std::vector<uint8_t>& data);
    
    // JSON test data
    static nlohmann::json createTestTelemetryData();
    static nlohmann::json createTestCommandData();
    static nlohmann::json createTestDebugData();

    static Message createTestBinaryMessage();
    static Message createTestJsonMessage();
    static Message createTestTextMessage();
};

// Test assertions
class TestAssertions {
public:
    static void assertMessageEquals(const Message& expected, const Message& actual);
    static void assertBasicPropertiesEquals(const MessageProperties& expected, const MessageProperties& actual);
    static void assertStatsIncreased(uint64_t before, uint64_t after, const std::string& metric);
    // Removed these methods since they're commented out in the implementation:
    // static void assertConnectionState(ConnectionState expected, ConnectionState actual);
    // static void assertChannelState(ChannelState expected, ChannelState actual);
    static void assertMessageContentEquals(const Message& expected, const Message& actual);
    static void assertMessagePropertiesEqual(const Message& expected, const Message& actual);
};

// Performance test helpers
class PerformanceHelper {
public:
    struct Timing {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
        std::chrono::duration<double> duration() const { return end - start; }
        double seconds() const { return duration().count(); }
        double milliseconds() const { return duration().count() * 1000.0; }
    };
    
    static Timing timeOperation(std::function<void()> operation);
    static void logPerformanceResults(const std::string& operation, const Timing& timing, size_t itemCount = 1);
    static void assertPerformance(const Timing& timing, std::chrono::milliseconds maxTime, const std::string& operation);
};

// Resource cleanup helper
class ResourceGuard {
public:
    explicit ResourceGuard(std::function<void()> cleanup);
    ~ResourceGuard();
    
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;
    ResourceGuard(ResourceGuard&&) = default;
    ResourceGuard& operator=(ResourceGuard&&) = default;
    
    void release();

private:
    std::function<void()> cleanup_;
    bool released_;
};

// Test helpers for generating random data
class TestHelpers {
public:
    static std::string generateRandomImei();
    static std::string generateRandomString(size_t length);
    static std::vector<uint8_t> generateRandomBinary(size_t length);
};

} // namespace test
} // namespace rabbitmq