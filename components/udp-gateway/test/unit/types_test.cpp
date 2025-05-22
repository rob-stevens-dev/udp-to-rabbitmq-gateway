#include "udp_gateway/types.hpp"
#include <gtest/gtest.h>

using namespace udp_gateway;

// Test basic types and enums
TEST(TypesTest, ErrorCodeToString) {
    EXPECT_EQ("SUCCESS", errorCodeToString(ErrorCode::SUCCESS));
    EXPECT_EQ("INVALID_MESSAGE", errorCodeToString(ErrorCode::INVALID_MESSAGE));
    EXPECT_EQ("AUTHENTICATION_FAILURE", errorCodeToString(ErrorCode::AUTHENTICATION_FAILURE));
    EXPECT_EQ("RATE_LIMITED", errorCodeToString(ErrorCode::RATE_LIMITED));
    EXPECT_EQ("DUPLICATE_MESSAGE", errorCodeToString(ErrorCode::DUPLICATE_MESSAGE));
    EXPECT_EQ("DEVICE_SUSPENDED", errorCodeToString(ErrorCode::DEVICE_SUSPENDED));
    EXPECT_EQ("INTERNAL_ERROR", errorCodeToString(ErrorCode::INTERNAL_ERROR));
    EXPECT_EQ("DEPENDENCY_FAILURE", errorCodeToString(ErrorCode::DEPENDENCY_FAILURE));
    EXPECT_EQ("INVALID_CONFIGURATION", errorCodeToString(ErrorCode::INVALID_CONFIGURATION));
}

// Test Result template
TEST(TypesTest, ResultSuccess) {
    auto result = Result<int>::ok(42);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(ErrorCode::SUCCESS, result.errorCode);
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_EQ(42, result.value);
    
    // Test bool conversion
    EXPECT_TRUE(result);
}

TEST(TypesTest, ResultError) {
    auto result = Result<int>::error(ErrorCode::INVALID_MESSAGE, "Test error");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(ErrorCode::INVALID_MESSAGE, result.errorCode);
    EXPECT_EQ("Test error", result.errorMessage);
    
    // Test bool conversion
    EXPECT_FALSE(result);
}

TEST(TypesTest, VoidResult) {
    auto successResult = makeSuccessResult();
    EXPECT_TRUE(successResult);
    EXPECT_TRUE(successResult.success);
    EXPECT_EQ(ErrorCode::SUCCESS, successResult.errorCode);
    
    auto errorResult = makeErrorResult(ErrorCode::INTERNAL_ERROR, "Test void error");
    EXPECT_FALSE(errorResult);
    EXPECT_FALSE(errorResult.success);
    EXPECT_EQ(ErrorCode::INTERNAL_ERROR, errorResult.errorCode);
    EXPECT_EQ("Test void error", errorResult.errorMessage);
}

// Test DeviceMessage structure
TEST(TypesTest, DeviceMessage) {
    DeviceMessage message;
    message.deviceId = "123456789012345";
    message.region = "na";
    message.payload = {0x01, 0x02, 0x03};
    message.sequenceNumber = 42;
    message.messageId = "msg-123";
    message.priority = Priority::HIGH;
    message.requiresAcknowledgment = true;
    
    EXPECT_EQ("123456789012345", message.deviceId);
    EXPECT_EQ("na", message.region);
    EXPECT_EQ(3, message.payload.size());
    EXPECT_EQ(42, message.sequenceNumber);
    EXPECT_EQ("msg-123", message.messageId);
    EXPECT_EQ(Priority::HIGH, message.priority);
    EXPECT_TRUE(message.requiresAcknowledgment);
}

// Test MessageAcknowledgment structure
TEST(TypesTest, MessageAcknowledgment) {
    MessageAcknowledgment ack;
    ack.deviceId = "123456789012345";
    ack.messageId = "msg-123";
    ack.success = false;
    ack.errorReason = "Test error";
    
    EXPECT_EQ("123456789012345", ack.deviceId);
    EXPECT_EQ("msg-123", ack.messageId);
    EXPECT_FALSE(ack.success);
    EXPECT_EQ("Test error", ack.errorReason);
}

// Test DeviceCommand structure
TEST(TypesTest, DeviceCommand) {
    DeviceCommand command;
    command.deviceId = "123456789012345";
    command.region = "eu";
    command.payload = {0x04, 0x05, 0x06};
    command.commandId = "cmd-456";
    command.priority = Priority::CRITICAL;
    command.delivered = false;
    command.acknowledged = false;
    
    EXPECT_EQ("123456789012345", command.deviceId);
    EXPECT_EQ("eu", command.region);
    EXPECT_EQ(3, command.payload.size());
    EXPECT_EQ("cmd-456", command.commandId);
    EXPECT_EQ(Priority::CRITICAL, command.priority);
    EXPECT_FALSE(command.delivered);
    EXPECT_FALSE(command.acknowledged);
}

// Test GatewayMetrics structure
TEST(TypesTest, GatewayMetrics) {
    GatewayMetrics metrics;
    metrics.totalPacketsReceived = 100;
    metrics.totalPacketsProcessed = 95;
    metrics.totalPacketsRejected = 5;
    metrics.totalPacketsDuplicated = 2;
    metrics.totalBytesReceived = 8192;
    metrics.activeConnections = 10;
    metrics.peakConnections = 15;
    
    EXPECT_EQ(100, metrics.totalPacketsReceived);
    EXPECT_EQ(95, metrics.totalPacketsProcessed);
    EXPECT_EQ(5, metrics.totalPacketsRejected);
    EXPECT_EQ(2, metrics.totalPacketsDuplicated);
    EXPECT_EQ(8192, metrics.totalBytesReceived);
    EXPECT_EQ(10, metrics.activeConnections);
    EXPECT_EQ(15, metrics.peakConnections);
}