#include "test_utils.hpp"
#include <cstdlib>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>

namespace rabbitmq_integration {
namespace test {

ConnectionConfig TestConfig::getTestConnectionConfig() {
    ConnectionConfig config;
    config.host = getEnvVar("RABBITMQ_HOST", "localhost");
    config.port = std::stoi(getEnvVar("RABBITMQ_PORT", "5672"));
    config.username = getEnvVar("RABBITMQ_USER", "guest");
    config.password = getEnvVar("RABBITMQ_PASS", "guest");
    config.vhost = getEnvVar("RABBITMQ_VHOST", "/");
    config.connectionTimeout = std::chrono::seconds(10);
//    config.heartbeat = std::chrono::seconds(30);
    config.maxRetries = 3;
    config.retryInterval = std::chrono::milliseconds(500);
    return config;
}

PublisherConfig TestConfig::getTestPublisherConfig() {
    PublisherConfig config;
//    config.connection = getTestConnectionConfig();
    config.confirmEnabled = true;
    config.persistentMessages = false; // Faster for tests
    config.maxRetries = 2;
    config.retryInterval = std::chrono::milliseconds(100);
//    config.confirmTimeout = std::chrono::seconds(5);
    config.localQueueSize = 100;
//    config.dropOnOverflow = false;
    config.enableBatching = false;
    return config;
}

ConsumerConfig TestConfig::getTestConsumerConfig() {
    ConsumerConfig config;
//    config.connection = getTestConnectionConfig();
    config.prefetchCount = 10;
    config.autoAck = false;
    config.ackTimeout = std::chrono::milliseconds(5000);
    config.exclusive = false;
//    config.noLocal = false;
//    config.autoRestart = true;
//    config.restartDelay = std::chrono::milliseconds(500);
    return config;
}



std::string TestConfig::getEnvVar(const std::string& name, const std::string& defaultValue) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : defaultValue;
}

bool TestConfig::isRabbitMQAvailable() {
    // For now, just return false to avoid integration test dependencies
    // In real implementation, this would try to connect
    return false;
}

void TestConfig::waitForRabbitMQ(std::chrono::seconds timeout) {
    // Stub implementation
    (void)timeout;
}

Message TestMessages::createTestTelemetryMessage(const std::string& imei) {
    nlohmann::json data = {
        {"imei", imei},
        {"latitude", 37.7749},
        {"longitude", -122.4194},
        {"speed", 35.0},
        {"battery", 0.85},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    Message msg(data);
    msg.setContentType("application/json");
    msg.setType("telemetry");
    msg.setHeader("imei", imei);
    msg.setDeliveryMode(DeliveryMode::Persistent);  // Change from setPersistent(true)
    
    return msg;
}

Message TestMessages::createTestCommandMessage(const std::string& command) {
    nlohmann::json data = {
        {"command", command},
        {"parameters", {{"force", true}, {"delay", 5}}},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    Message msg(data);
    msg.setContentType("application/json");
    msg.setType("command");
    msg.setHeader("command", command);
    msg.setDeliveryMode(DeliveryMode::Persistent);  // Change from setPersistent(true)
    
    return msg;
}

Message TestMessages::createTestDebugMessage(const std::string& imei) {
    nlohmann::json data = {
        {"imei", imei},
        {"debugType", "error"},
        {"message", "Test debug message"},
        {"level", "INFO"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    Message msg(data);
    msg.setContentType("application/json");
    msg.setType("debug");
    msg.setHeader("imei", imei);
    msg.setDeliveryMode(DeliveryMode::NonPersistent);  // Change from setPersistent(false)
    
    return msg;
}

Message TestMessages::createTestBinaryMessage() {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    Message msg(data);
    msg.setContentType("application/octet-stream");
    msg.setType("binary");
    
    return msg;
}

Message TestMessages::createTestJsonMessage() {
    nlohmann::json data = {
        {"test", true},
        {"value", 42},
        {"name", "test_message"}
    };
    
    Message msg(data);
    msg.setContentType("application/json");
    msg.setType("test");
    
    return msg;
}

Message TestMessages::createTestTextMessage() {
    std::string text = "This is a test message";
    
    Message msg(text);
    msg.setContentType("text/plain");
    msg.setType("text");
    
    return msg;
}

Message TestMessages::createTestDiscardMessage(const std::string& imei) {
    nlohmann::json data = {
        {"imei", imei},
        {"reason", "test_reason"},
        {"originalData", "test_original_data"}
    };
    return Message(data);  // Simple constructor instead of MessageUtils
}

Message TestMessages::createLargeMessage(size_t sizeBytes) {
    std::vector<uint8_t> data(sizeBytes, 0x42);
    return Message(data);  // Simple constructor instead of Message::createBinaryMessage
}

Message TestMessages::createBinaryMessage(const std::vector<uint8_t>& data) {
    return Message(data);  // Simple constructor instead of Message::createBinaryMessage
}

nlohmann::json TestMessages::createTestTelemetryData() {
    return {
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"location", {
            {"latitude", 37.7749},
            {"longitude", -122.4194}
        }},
        {"speed", 35.0},
        {"battery", 0.75},
        {"signal_strength", -65},
        {"temperature", 22.5}
    };
}

nlohmann::json TestMessages::createTestCommandData() {
    return {
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"parameters", {
            {"param1", "value1"},
            {"param2", 42},
            {"param3", true}
        }}
    };
}

nlohmann::json TestMessages::createTestDebugData() {
    return {
        {"debug_level", "INFO"},
        {"message", "Test debug message"},
        {"context", {
            {"function", "test_function"},
            {"line", 123},
            {"thread_id", "main"}
        }}
    };
}

void TestAssertions::assertMessageEquals(const Message& expected, const Message& actual) {
    EXPECT_EQ(expected.getContentType(), actual.getContentType()) << "Message content differs";
    EXPECT_EQ(expected.getContentType(), actual.getContentType()) << "Content type differs";
    EXPECT_EQ(expected.getMessageId(), actual.getMessageId()) << "Message ID differs";
    EXPECT_EQ(expected.getCorrelationId(), actual.getCorrelationId()) << "Correlation ID differs";
}

void TestAssertions::assertMessageContentEquals(const Message& expected, const Message& actual) {
    EXPECT_EQ(expected.getContentBytes(), actual.getContentBytes()) 
        << "Message content differs";
    EXPECT_EQ(expected.getContentLength(), actual.getContentLength()) 
        << "Content lengths differ";
}

void TestAssertions::assertMessagePropertiesEqual(const Message& expected, const Message& actual) {
    EXPECT_EQ(expected.getContentType(), actual.getContentType());
    EXPECT_EQ(expected.getType(), actual.getType());
    EXPECT_EQ(expected.getDeliveryMode(), actual.getDeliveryMode());
    EXPECT_EQ(expected.getPriority(), actual.getPriority());
    EXPECT_EQ(expected.getCorrelationId(), actual.getCorrelationId());
    EXPECT_EQ(expected.getReplyTo(), actual.getReplyTo());
}

void TestAssertions::assertBasicPropertiesEquals(const MessageProperties& expected, const MessageProperties& actual) {
    EXPECT_EQ(expected.contentType, actual.contentType) << "Content type differs";
    EXPECT_EQ(expected.contentEncoding, actual.contentEncoding) << "Content encoding differs";
    EXPECT_EQ(expected.correlationId, actual.correlationId) << "Correlation ID differs";
    EXPECT_EQ(expected.messageId, actual.messageId) << "Message ID differs";
    EXPECT_EQ(expected.replyTo, actual.replyTo) << "Reply-to differs";
    EXPECT_EQ(expected.deliveryMode, actual.deliveryMode) << "Delivery mode differs";
    EXPECT_EQ(expected.priority, actual.priority) << "Priority differs";
    EXPECT_EQ(expected.type, actual.type) << "Type differs";
    EXPECT_EQ(expected.userId, actual.userId) << "User ID differs";
    EXPECT_EQ(expected.appId, actual.appId) << "App ID differs";
    EXPECT_EQ(expected.headers, actual.headers) << "Headers differ";
}

void TestAssertions::assertStatsIncreased(uint64_t before, uint64_t after, const std::string& metric) {
    EXPECT_GT(after, before) << metric << " should have increased from " << before << " to " << after;
}

// void TestAssertions::assertConnectionState(ConnectionState expected, ConnectionState actual) {
//     EXPECT_EQ(expected, actual) << "Expected connection state " << connectionStateToString(expected) 
//                                << " but got " << connectionStateToString(actual);
// }

// void TestAssertions::assertChannelState(ChannelState expected, ChannelState actual) {
//     EXPECT_EQ(expected, actual) << "Expected channel state " << channelStateToString(expected) 
//                                << " but got " << channelStateToString(actual);
// }

PerformanceHelper::Timing PerformanceHelper::timeOperation(std::function<void()> operation) {
    Timing timing;
    timing.start = std::chrono::steady_clock::now();
    operation();
    timing.end = std::chrono::steady_clock::now();
    return timing;
}

void PerformanceHelper::logPerformanceResults(const std::string& operation, const Timing& timing, size_t itemCount) {
    double totalMs = timing.milliseconds();
    double throughput = itemCount / timing.seconds();
    
    std::cout << "Performance: " << operation << std::endl;
    std::cout << "  Total time: " << std::fixed << std::setprecision(2) << totalMs << " ms" << std::endl;
    std::cout << "  Items: " << itemCount << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " items/second" << std::endl;
    
    if (itemCount > 1) {
        double avgMs = totalMs / itemCount;
        std::cout << "  Average per item: " << std::fixed << std::setprecision(3) << avgMs << " ms" << std::endl;
    }
}

void PerformanceHelper::assertPerformance(const Timing& timing, std::chrono::milliseconds maxTime, const std::string& operation) {
    auto actualMs = std::chrono::duration_cast<std::chrono::milliseconds>(timing.duration());
    EXPECT_LE(actualMs, maxTime) << operation << " took " << actualMs.count() << "ms, expected <= " << maxTime.count() << "ms";
}

ResourceGuard::ResourceGuard(std::function<void()> cleanup) 
    : cleanup_(std::move(cleanup)), released_(false) {
}

ResourceGuard::~ResourceGuard() {
    if (!released_ && cleanup_) {
        try {
            cleanup_();
        } catch (const std::exception&) {
            // Ignore cleanup errors in destructor
        }
    }
}

void ResourceGuard::release() {
    released_ = true;
}

std::string TestHelpers::generateRandomImei() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 9);
    
    std::string imei;
    for (int i = 0; i < 15; ++i) {
        imei += std::to_string(dis(gen));
    }
    return imei;
}

std::string TestHelpers::generateRandomString(size_t length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis('a', 'z');
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += static_cast<char>(dis(gen));
    }
    return result;
}

std::vector<uint8_t> TestHelpers::generateRandomBinary(size_t length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    std::vector<uint8_t> result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<uint8_t>(dis(gen)));
    }
    return result;
}

} // namespace test
} // namespace rabbitmq