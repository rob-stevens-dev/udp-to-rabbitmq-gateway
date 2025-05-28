#include <gtest/gtest.h>
#include "rabbitmq_integration/message.hpp"
#include "utils/test_utils.hpp"
#include <nlohmann/json.hpp>

using namespace rabbitmq_integration;
using namespace rabbitmq_integration::test;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test data
        testJson_ = {
            {"name", "test"},
            {"value", 42},
            {"active", true}
        };
        
        testText_ = "Hello, RabbitMQ!";
        testBinary_ = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    void TearDown() override {
        // Common test cleanup
    }

protected:
    nlohmann::json testJson_;
    std::string testText_;
    std::vector<uint8_t> testBinary_;
};

// Basic constructor tests
TEST_F(MessageTest, DefaultConstructor) {
    Message msg;
    
    EXPECT_TRUE(msg.isEmpty());
    EXPECT_EQ(0, msg.getContentLength());
    EXPECT_TRUE(msg.getContentBytes().empty());
    EXPECT_EQ("application/octet-stream", msg.getContentType());
}

TEST_F(MessageTest, JsonConstructor) {
    Message msg(testJson_);
    
    EXPECT_FALSE(msg.isEmpty());
    EXPECT_GT(msg.getContentLength(), 0);
    EXPECT_EQ("application/json", msg.getContentType());
    EXPECT_TRUE(msg.isJson());
    EXPECT_TRUE(msg.isText());
    
    auto retrievedJson = msg.getContentJson();
    EXPECT_EQ(testJson_, retrievedJson);
}

TEST_F(MessageTest, TextConstructor) {
    Message msg(testText_);
    
    EXPECT_FALSE(msg.isEmpty());
    EXPECT_EQ(testText_.length(), msg.getContentLength());
    EXPECT_EQ("text/plain", msg.getContentType());
    EXPECT_FALSE(msg.isJson());
    EXPECT_TRUE(msg.isText());
    
    EXPECT_EQ(testText_, msg.getContentString());
}

TEST_F(MessageTest, BinaryConstructor) {
    Message msg(testBinary_);
    
    EXPECT_FALSE(msg.isEmpty());
    EXPECT_EQ(testBinary_.size(), msg.getContentLength());
    EXPECT_EQ("application/octet-stream", msg.getContentType());
    EXPECT_FALSE(msg.isJson());
    EXPECT_FALSE(msg.isText());
    
    EXPECT_EQ(testBinary_, msg.getContentBytes());
}

TEST_F(MessageTest, PointerConstructor) {
    Message msg(testBinary_.data(), testBinary_.size());
    
    EXPECT_EQ(testBinary_.size(), msg.getContentLength());
    EXPECT_EQ(testBinary_, msg.getContentBytes());
}

// Copy and move semantics
TEST_F(MessageTest, CopyConstructor) {
    Message original(testJson_);
    original.setMessageId("test-id");
    original.setCorrelationId("test-correlation");
    
    Message copy(original);
    
    EXPECT_EQ(original.getContentBytes(), copy.getContentBytes());
    EXPECT_EQ(original.getContentType(), copy.getContentType());
    EXPECT_EQ(original.getMessageId(), copy.getMessageId());
    EXPECT_EQ(original.getCorrelationId(), copy.getCorrelationId());
}

TEST_F(MessageTest, MoveConstructor) {
    Message original(testJson_);
    original.setMessageId("test-id");
    
    std::string originalId = original.getMessageId();
    auto originalContent = original.getContentBytes();
    
    Message moved(std::move(original));
    
    EXPECT_EQ(originalContent, moved.getContentBytes());
    EXPECT_EQ(originalId, moved.getMessageId());
}

TEST_F(MessageTest, CopyAssignment) {
    Message original(testJson_);
    original.setMessageId("test-id");
    
    Message copy;
    copy = original;
    
    EXPECT_EQ(original.getContentBytes(), copy.getContentBytes());
    EXPECT_EQ(original.getMessageId(), copy.getMessageId());
}

TEST_F(MessageTest, MoveAssignment) {
    Message original(testJson_);
    original.setMessageId("test-id");
    
    auto originalContent = original.getContentBytes();
    std::string originalId = original.getMessageId();
    
    Message moved;
    moved = std::move(original);
    
    EXPECT_EQ(originalContent, moved.getContentBytes());
    EXPECT_EQ(originalId, moved.getMessageId());
}

// Content manipulation
TEST_F(MessageTest, SetJsonContent) {
    Message msg;
    msg.setContent(testJson_);
    
    EXPECT_EQ("application/json", msg.getContentType());
    EXPECT_TRUE(msg.isJson());
    EXPECT_EQ(testJson_, msg.getContentJson());
}

TEST_F(MessageTest, SetTextContent) {
    Message msg;
    msg.setContent(testText_);
    
    EXPECT_EQ("text/plain", msg.getContentType());
    EXPECT_TRUE(msg.isText());
    EXPECT_EQ(testText_, msg.getContentString());
}

TEST_F(MessageTest, SetBinaryContent) {
    Message msg;
    msg.setContent(testBinary_);
    
    EXPECT_EQ("application/octet-stream", msg.getContentType());
    EXPECT_FALSE(msg.isText());
    EXPECT_EQ(testBinary_, msg.getContentBytes());
}

TEST_F(MessageTest, SetPointerContent) {
    Message msg;
    msg.setContent(testBinary_.data(), testBinary_.size());
    
    EXPECT_EQ(testBinary_.size(), msg.getContentLength());
    EXPECT_EQ(testBinary_, msg.getContentBytes());
}

// Properties tests
TEST_F(MessageTest, BasicProperties) {
    Message msg(testText_);
    
    // Content type
    msg.setContentType("text/html");
    EXPECT_EQ("text/html", msg.getContentType());
    
    // Content encoding
    msg.setContentEncoding("utf-8");
    EXPECT_EQ("utf-8", msg.getContentEncoding());
    
    // Message ID
    msg.setMessageId("msg-123");
    EXPECT_EQ("msg-123", msg.getMessageId());
    
    // Correlation ID
    msg.setCorrelationId("corr-456");
    EXPECT_EQ("corr-456", msg.getCorrelationId());
    
    // Reply-to
    msg.setReplyTo("reply-queue");
    EXPECT_EQ("reply-queue", msg.getReplyTo());
    
    // Delivery mode
    msg.setDeliveryMode(DeliveryMode::Persistent);
    EXPECT_EQ(DeliveryMode::Persistent, msg.getDeliveryMode());
    
    // Priority
    msg.setPriority(5);
    EXPECT_EQ(Priority::High, msg.getPriority()); // or whatever Priority enum value corresponds to 5
    
    // Type
    msg.setType("test-message");
    EXPECT_EQ("test-message", msg.getType());
    
    // User ID
    msg.setUserId("test-user");
    EXPECT_EQ("test-user", msg.getUserId());
    
    // App ID
    msg.setAppId("test-app");
    EXPECT_EQ("test-app", msg.getAppId());
}

TEST_F(MessageTest, PriorityLimits) {
    Message msg;
    
    msg.setPriority(Priority::Urgent); // Use highest valid priority
    EXPECT_EQ(Priority::Urgent, msg.getPriority());
    
    msg.setPriority(Priority::Normal);
    EXPECT_EQ(Priority::Normal, msg.getPriority());
}

TEST_F(MessageTest, ExpirationHandling) {
    Message msg;
    
    auto ttl = std::chrono::milliseconds(5000);
    msg.setExpiration(ttl);
    auto retrievedTtl = msg.getExpiration();
    EXPECT_EQ(ttl.count(), retrievedTtl.count());
    
    // Test zero expiration
    msg.setExpiration(std::chrono::milliseconds(0));
    EXPECT_EQ(std::chrono::milliseconds(0), msg.getExpiration());
}

TEST_F(MessageTest, TimestampHandling) {
    Message msg;
    
    auto now = std::chrono::system_clock::now();
    msg.setTimestamp(now);
    
    auto retrieved = msg.getTimestamp();
    
    // Calculate the difference and get absolute value correctly
    auto diffDuration = now - retrieved;
    auto diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(diffDuration);
    auto absDiffCount = std::abs(diffMs.count());
    
    EXPECT_LT(absDiffCount, 1000); // Should be within 1 second
}

// Headers tests
TEST_F(MessageTest, HeaderManagement) {
    Message msg;
    
    // Set headers
    msg.setHeader("key1", "value1");
    msg.setHeader("key2", "value2");
    
    // Get headers
    EXPECT_EQ("value1", msg.getHeader("key1"));
    EXPECT_EQ("value2", msg.getHeader("key2"));
    EXPECT_EQ("", msg.getHeader("nonexistent"));
    
    // Check existence
    EXPECT_TRUE(msg.hasHeader("key1"));
    EXPECT_FALSE(msg.hasHeader("nonexistent"));
    
    // Get all headers
    auto headers = msg.getHeaders();
    EXPECT_EQ(2, headers.size());
    EXPECT_EQ("value1", headers.at("key1"));
    EXPECT_EQ("value2", headers.at("key2"));
    
    // Remove header
    msg.removeHeader("key1");
    EXPECT_FALSE(msg.hasHeader("key1"));
    EXPECT_TRUE(msg.hasHeader("key2"));
    
    // Set multiple headers
    Properties newHeaders = {{"key3", "value3"}, {"key4", "value4"}};
    msg.setHeaders(newHeaders);
    
    auto allHeaders = msg.getHeaders();
    EXPECT_EQ(3, allHeaders.size()); // key2 + key3 + key4
    EXPECT_TRUE(msg.hasHeader("key2")); // Should still exist
    EXPECT_TRUE(msg.hasHeader("key3"));
    EXPECT_TRUE(msg.hasHeader("key4"));
}

// Utility methods
TEST_F(MessageTest, Clear) {
    Message msg(testJson_);
    msg.setMessageId("test-id");
    msg.setHeader("key", "value");
    
    EXPECT_FALSE(msg.isEmpty());
    EXPECT_FALSE(msg.getMessageId().empty());
    EXPECT_TRUE(msg.hasHeader("key"));
    
    msg.clear();
    
    EXPECT_TRUE(msg.isEmpty());
    EXPECT_TRUE(msg.getMessageId().empty());
    EXPECT_FALSE(msg.hasHeader("key"));
}

TEST_F(MessageTest, EstimateSize) {
    Message msg(testText_);
    msg.setMessageId("test-id-123");
    msg.setCorrelationId("corr-456");
    msg.setHeader("key1", "value1");
    
    size_t estimatedSize = msg.estimateSize();
    
    // Should be at least the content size
    EXPECT_GE(estimatedSize, testText_.size());
    
    // Should include property sizes
    EXPECT_GT(estimatedSize, testText_.size());
}

TEST_F(MessageTest, ToString) {
    Message msg(testText_);
    msg.setMessageId("test-id");
    msg.setCorrelationId("test-corr");
    
    std::string str = msg.toString();
    
    EXPECT_NE(std::string::npos, str.find("Message["));
    EXPECT_NE(std::string::npos, str.find("size="));
    EXPECT_NE(std::string::npos, str.find("messageId=test-id"));
    EXPECT_NE(std::string::npos, str.find("correlationId=test-corr"));
}

TEST_F(MessageTest, ToJsonSerialization) {
    Message msg(testJson_);
    msg.setMessageId("test-id");
    msg.setCorrelationId("test-corr");
    msg.setContentType("application/json");
    msg.setHeader("custom", "header");
    
    auto json = msg.toJson();
    
    EXPECT_TRUE(json.contains("content"));
    EXPECT_TRUE(json.contains("properties"));
    
    auto props = json["properties"];
    EXPECT_EQ("test-id", props["messageId"]);
    EXPECT_EQ("test-corr", props["correlationId"]);
    EXPECT_EQ("application/json", props["contentType"]);
    
    EXPECT_TRUE(props.contains("headers"));
    EXPECT_EQ("header", props["headers"]["custom"]);
}

// Comparison operators
TEST_F(MessageTest, EqualityOperators) {
    Message msg1(testText_);
    msg1.setMessageId("same-id");
    msg1.setContentType("text/plain");
    
    Message msg2(testText_);
    msg2.setMessageId("same-id");
    msg2.setContentType("text/plain");
    
    Message msg3(testJson_);
    msg3.setMessageId("different-id");
    
    EXPECT_TRUE(msg1 == msg2);
    EXPECT_FALSE(msg1 != msg2);
    
    EXPECT_FALSE(msg1 == msg3);
    EXPECT_TRUE(msg1 != msg3);
}

// Static factory methods
TEST_F(MessageTest, StaticFactoryMethods) {
    // JSON message
    auto jsonMsg = Message::createJsonMessage(testJson_);
    EXPECT_EQ("application/json", jsonMsg.getContentType());
    EXPECT_FALSE(jsonMsg.getMessageId().empty());
    EXPECT_TRUE(jsonMsg.isJson());
    
    // Text message
    auto textMsg = Message::createTextMessage(testText_);
    EXPECT_EQ("text/plain", textMsg.getContentType());
    EXPECT_FALSE(textMsg.getMessageId().empty());
    EXPECT_TRUE(textMsg.isText());
    
    // Binary message
    auto binaryMsg = Message::createBinaryMessage(testBinary_);
    EXPECT_EQ("application/octet-stream", binaryMsg.getContentType());
    EXPECT_FALSE(binaryMsg.getMessageId().empty());
    EXPECT_FALSE(binaryMsg.isText());
}

// Error handling
TEST_F(MessageTest, InvalidJsonContent) {
    std::string invalidJson = "invalid json content";
    Message msg(invalidJson);
    msg.setContentType("application/json");
    
    EXPECT_THROW(msg.getContentJson(), std::invalid_argument);
}

// MessageUtils tests
class MessageUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        testImei_ = "123456789012345";
    }

protected:
    std::string testImei_;
};

TEST_F(MessageUtilsTest, CreateTelemetryMessage) {
    double lat = 37.7749;
    double lon = -122.4194;
    double speed = 35.0;
    double battery = 0.75;
    nlohmann::json custom = {{"temperature", 22.5}};
    
    auto msg = MessageUtils::createTelemetryMessage(testImei_, lat, lon, speed, battery, custom);
    
    EXPECT_TRUE(msg.isJson());
    EXPECT_EQ("telemetry", msg.getType());
    EXPECT_EQ(testImei_, msg.getHeader("imei"));
    EXPECT_EQ(DeliveryMode::Persistent, msg.getDeliveryMode());
    
    auto content = msg.getContentJson();
    EXPECT_EQ(testImei_, content["imei"]);
    EXPECT_EQ(lat, content["location"]["latitude"]);
    EXPECT_EQ(lon, content["location"]["longitude"]);
    EXPECT_EQ(speed, content["speed"]);
    EXPECT_EQ(battery, content["battery"]);
    EXPECT_EQ(22.5, content["custom"]["temperature"]);
    EXPECT_TRUE(content.contains("timestamp"));
}

TEST_F(MessageUtilsTest, CreateCommandMessage) {
    std::string command = "restart";
    nlohmann::json params = {{"force", true}, {"delay", 5}};
    std::string corrId = "cmd-123";
    
    auto msg = MessageUtils::createCommandMessage(command, params, corrId);
    
    EXPECT_TRUE(msg.isJson());
    EXPECT_EQ("command", msg.getType());
    EXPECT_EQ(corrId, msg.getCorrelationId());
    EXPECT_EQ(DeliveryMode::Persistent, msg.getDeliveryMode());
    
    auto content = msg.getContentJson();
    EXPECT_EQ(command, content["command"]);
    EXPECT_TRUE(content["parameters"]["force"]);
    EXPECT_EQ(5, content["parameters"]["delay"]);
    EXPECT_TRUE(content.contains("timestamp"));
}

TEST_F(MessageUtilsTest, CreateDebugMessage) {
    std::string debugType = "error";
    nlohmann::json debugData = {{"error", "Connection failed"}, {"code", 500}};
    
    auto msg = MessageUtils::createDebugMessage(testImei_, debugType, debugData);
    
    EXPECT_TRUE(msg.isJson());
    EXPECT_EQ("debug", msg.getType());
    EXPECT_EQ(testImei_, msg.getHeader("imei"));
    EXPECT_EQ(debugType, msg.getHeader("debugType"));
    
    auto content = msg.getContentJson();
    EXPECT_EQ(testImei_, content["imei"]);
    EXPECT_EQ(debugType, content["debugType"]);
    EXPECT_EQ("Connection failed", content["data"]["error"]);
    EXPECT_EQ(500, content["data"]["code"]);
}

TEST_F(MessageUtilsTest, CreateDiscardMessage) {
    std::string reason = "Invalid format";
    std::string originalData = "corrupted data";
    
    auto msg = MessageUtils::createDiscardMessage(testImei_, reason, originalData);
    
    EXPECT_TRUE(msg.isJson());
    EXPECT_EQ("discard", msg.getType());
    EXPECT_EQ(testImei_, msg.getHeader("imei"));
    EXPECT_EQ(reason, msg.getHeader("reason"));
    
    auto content = msg.getContentJson();
    EXPECT_EQ(testImei_, content["imei"]);
    EXPECT_EQ(reason, content["reason"]);
    EXPECT_EQ(originalData, content["originalData"]);
}

TEST_F(MessageUtilsTest, MessageValidation) {
    auto telemetryMsg = MessageUtils::createTelemetryMessage(testImei_, 37.7749, -122.4194);
    auto commandMsg = MessageUtils::createCommandMessage("test_command");
    auto invalidMsg = Message::createTextMessage("not json");
    
    EXPECT_TRUE(MessageUtils::isValidTelemetryMessage(telemetryMsg));
    EXPECT_FALSE(MessageUtils::isValidTelemetryMessage(commandMsg));
    EXPECT_FALSE(MessageUtils::isValidTelemetryMessage(invalidMsg));
    
    EXPECT_TRUE(MessageUtils::isValidCommandMessage(commandMsg));
    EXPECT_FALSE(MessageUtils::isValidCommandMessage(telemetryMsg));
    EXPECT_FALSE(MessageUtils::isValidCommandMessage(invalidMsg));
}

TEST_F(MessageUtilsTest, ExtractImei) {
    // From header
    auto msg1 = Message::createTextMessage("test");
    msg1.setHeader("imei", testImei_);
    EXPECT_EQ(testImei_, MessageUtils::extractImei(msg1));
    
    // From JSON content
    auto msg2 = MessageUtils::createTelemetryMessage(testImei_, 0, 0);
    EXPECT_EQ(testImei_, MessageUtils::extractImei(msg2));
    
    // Not found
    auto msg3 = Message::createTextMessage("test");
    EXPECT_EQ("", MessageUtils::extractImei(msg3));
}

TEST_F(MessageUtilsTest, GenerateIds) {
    auto corrId1 = MessageUtils::generateCorrelationId();
    auto corrId2 = MessageUtils::generateCorrelationId();
    EXPECT_NE(corrId1, corrId2);
    EXPECT_FALSE(corrId1.empty());
    
    auto msgId1 = MessageUtils::generateMessageId();
    auto msgId2 = MessageUtils::generateMessageId();
    EXPECT_NE(msgId1, msgId2);
    EXPECT_FALSE(msgId1.empty());
    
    // Message ID should contain timestamp
    EXPECT_NE(std::string::npos, msgId1.find("-"));
}