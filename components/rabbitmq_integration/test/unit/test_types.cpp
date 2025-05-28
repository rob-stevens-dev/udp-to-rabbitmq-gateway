// test/unit/test_types.cpp
#include <gtest/gtest.h>
#include "rabbitmq_integration/types.hpp"
#include "utils/test_utils.hpp"

using namespace rabbitmq_integration;
using namespace rabbitmq_integration::test;

class TypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }
};

// Test enum conversions
TEST_F(TypesTest, ExchangeTypeConversions) {
    EXPECT_EQ("direct", exchangeTypeToString(ExchangeType::Direct));
    EXPECT_EQ("fanout", exchangeTypeToString(ExchangeType::Fanout));
    EXPECT_EQ("topic", exchangeTypeToString(ExchangeType::Topic));
    EXPECT_EQ("headers", exchangeTypeToString(ExchangeType::Headers));
    
    EXPECT_EQ(ExchangeType::Direct, stringToExchangeType("direct"));
    EXPECT_EQ(ExchangeType::Fanout, stringToExchangeType("fanout"));
    EXPECT_EQ(ExchangeType::Topic, stringToExchangeType("topic"));
    EXPECT_EQ(ExchangeType::Headers, stringToExchangeType("headers"));
    
    // Test default for unknown
    EXPECT_EQ(ExchangeType::Direct, stringToExchangeType("unknown"));
}

TEST_F(TypesTest, ConnectionStateConversions) {
    EXPECT_EQ("Disconnected", connectionStateToString(ConnectionState::Disconnected));
    EXPECT_EQ("Connecting", connectionStateToString(ConnectionState::Connecting));
    EXPECT_EQ("Connected", connectionStateToString(ConnectionState::Connected));
    EXPECT_EQ("Disconnecting", connectionStateToString(ConnectionState::Disconnecting));
    EXPECT_EQ("Failed", connectionStateToString(ConnectionState::Failed));
}

TEST_F(TypesTest, ChannelStateConversions) {
    EXPECT_EQ("Closed", channelStateToString(ChannelState::Closed));
    EXPECT_EQ("Opening", channelStateToString(ChannelState::Opening));
    EXPECT_EQ("Open", channelStateToString(ChannelState::Open));
    EXPECT_EQ("Closing", channelStateToString(ChannelState::Closing));
    EXPECT_EQ("Failed", channelStateToString(ChannelState::Failed));
}

TEST_F(TypesTest, ErrorTypeConversions) {
    EXPECT_EQ("None", errorTypeToString(ErrorType::None));
    EXPECT_EQ("ConnectionError", errorTypeToString(ErrorType::ConnectionError));
    EXPECT_EQ("ChannelError", errorTypeToString(ErrorType::ChannelError));
    EXPECT_EQ("AuthenticationError", errorTypeToString(ErrorType::AuthenticationError));
    EXPECT_EQ("NetworkError", errorTypeToString(ErrorType::NetworkError));
    EXPECT_EQ("ProtocolError", errorTypeToString(ErrorType::ProtocolError));
    EXPECT_EQ("TimeoutError", errorTypeToString(ErrorType::TimeoutError));
    EXPECT_EQ("ResourceError", errorTypeToString(ErrorType::ResourceError));
}

TEST_F(TypesTest, AmqpErrorConversions) {
    EXPECT_EQ("OK", amqpErrorToString(AMQP_STATUS_OK));
    EXPECT_EQ("No memory", amqpErrorToString(AMQP_STATUS_NO_MEMORY));
    EXPECT_EQ("Bad AMQP data", amqpErrorToString(AMQP_STATUS_BAD_AMQP_DATA));
    EXPECT_EQ("Connection closed", amqpErrorToString(AMQP_STATUS_CONNECTION_CLOSED));
    EXPECT_EQ("Timeout", amqpErrorToString(AMQP_STATUS_TIMEOUT));
    
    // Test unknown error
    std::string unknownError = amqpErrorToString(-9999);
    EXPECT_NE(std::string::npos, unknownError.find("Unknown error"));
    EXPECT_NE(std::string::npos, unknownError.find("-9999"));
}

TEST_F(TypesTest, AmqpErrorToErrorType) {
    EXPECT_EQ(ErrorType::None, amqpErrorToErrorType(AMQP_STATUS_OK));
    EXPECT_EQ(ErrorType::ResourceError, amqpErrorToErrorType(AMQP_STATUS_NO_MEMORY));
    EXPECT_EQ(ErrorType::ProtocolError, amqpErrorToErrorType(AMQP_STATUS_BAD_AMQP_DATA));
    EXPECT_EQ(ErrorType::NetworkError, amqpErrorToErrorType(AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED));
    EXPECT_EQ(ErrorType::ConnectionError, amqpErrorToErrorType(AMQP_STATUS_CONNECTION_CLOSED));
    EXPECT_EQ(ErrorType::TimeoutError, amqpErrorToErrorType(AMQP_STATUS_TIMEOUT));
    EXPECT_EQ(ErrorType::AuthenticationError, amqpErrorToErrorType(AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD));
}

// Test Result template
TEST_F(TypesTest, ResultSuccess) {
    Result<int> result(42);
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result);
    EXPECT_EQ(42, result.value);
    EXPECT_EQ(42, *result);
    EXPECT_EQ(42, *result.operator->());
    EXPECT_EQ(ErrorType::None, result.error);
}

TEST_F(TypesTest, ResultFailure) {
    Result<int> result(ErrorType::NetworkError, "Connection failed");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result);
    EXPECT_EQ(ErrorType::NetworkError, result.error);
    EXPECT_EQ("Connection failed", result.message);
}

TEST_F(TypesTest, ResultVoidSuccess) {
    Result<void> result;
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result);
    EXPECT_EQ(ErrorType::None, result.error);
}

TEST_F(TypesTest, ResultVoidFailure) {
    Result<void> result(ErrorType::ChannelError, "Channel closed");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result);
    EXPECT_EQ(ErrorType::ChannelError, result.error);
    EXPECT_EQ("Channel closed", result.message);
}

// Test configuration structs
TEST_F(TypesTest, ConnectionConfig) {
    ConnectionConfig config;
    
    // Test defaults
    EXPECT_EQ("localhost", config.host);
    EXPECT_EQ(5672, config.port);
    EXPECT_EQ("/", config.vhost);
    EXPECT_EQ("guest", config.username);
    EXPECT_EQ("guest", config.password);
    EXPECT_FALSE(config.useTLS);
    EXPECT_EQ(std::chrono::seconds(30), config.connectionTimeout);
    EXPECT_EQ(std::chrono::seconds(60), config.heartbeat);
    EXPECT_EQ(3, config.maxRetries);
    EXPECT_EQ(std::chrono::seconds(5), config.retryInterval);
    EXPECT_TRUE(config.autoReconnect);
    EXPECT_EQ(131072U, config.frameMax);
    EXPECT_EQ(0U, config.channelMax);
}

TEST_F(TypesTest, PublisherConfig) {
    PublisherConfig config;
    
    // Test defaults
    EXPECT_TRUE(config.confirmEnabled);
    EXPECT_TRUE(config.persistentMessages);
    EXPECT_EQ(3, config.maxRetries);
    EXPECT_EQ(std::chrono::seconds(5), config.retryInterval);
    EXPECT_EQ(std::chrono::seconds(30), config.confirmTimeout);
    EXPECT_EQ(10000U, config.localQueueSize);
    EXPECT_FALSE(config.dropOnOverflow);
    EXPECT_FALSE(config.enableBatching);
    EXPECT_EQ(100, config.batchSize);
    EXPECT_EQ(std::chrono::milliseconds(100), config.batchTimeout);
}

TEST_F(TypesTest, ConsumerConfig) {
    ConsumerConfig config;
    
    // Test defaults
    EXPECT_EQ(10, config.prefetchCount);
    EXPECT_FALSE(config.autoAck);
    EXPECT_EQ(std::chrono::seconds(30), config.ackTimeout);
    EXPECT_FALSE(config.exclusive);
    EXPECT_FALSE(config.noLocal);
    EXPECT_TRUE(config.autoRestart);
    EXPECT_EQ(std::chrono::milliseconds(1000), config.restartDelay);
}

TEST_F(TypesTest, QueueConfig) {
    QueueConfig config;
    config.name = "test-queue";
    
    EXPECT_EQ("test-queue", config.name);
    EXPECT_TRUE(config.durable);
    EXPECT_FALSE(config.exclusive);
    EXPECT_FALSE(config.autoDelete);
    EXPECT_TRUE(config.arguments.empty());
    EXPECT_FALSE(config.messageTTL.has_value());
    EXPECT_FALSE(config.expires.has_value());
    EXPECT_FALSE(config.maxLength.has_value());
    EXPECT_FALSE(config.maxLengthBytes.has_value());
}

TEST_F(TypesTest, ExchangeConfig) {
    ExchangeConfig config;
    config.name = "test-exchange";
    config.type = "direct";  // Fix: assign string instead of enum
    
    EXPECT_EQ("test-exchange", config.name);
    EXPECT_EQ("direct", config.type);  // Fix: compare strings
    EXPECT_TRUE(config.durable);
    EXPECT_FALSE(config.autoDelete);
    EXPECT_FALSE(config.internal);
    EXPECT_TRUE(config.arguments.empty());
}

// Test statistics structures
TEST_F(TypesTest, ConnectionStats) {
    ConnectionStats stats;
    
    EXPECT_EQ(0U, stats.connectAttempts.load());
    EXPECT_EQ(0U, stats.successfulConnects.load());
    EXPECT_EQ(0U, stats.failedConnects.load());
    EXPECT_EQ(0U, stats.disconnects.load());
    EXPECT_EQ(0U, stats.reconnects.load());
    EXPECT_EQ(0U, stats.bytesReceived.load());
    EXPECT_EQ(0U, stats.bytesSent.load());
    
    // Test atomic operations
    stats.connectAttempts++;
    stats.successfulConnects += 5;
    
    EXPECT_EQ(1U, stats.connectAttempts.load());
    EXPECT_EQ(5U, stats.successfulConnects.load());
}

TEST_F(TypesTest, PublisherStats) {
    PublisherStats stats;
    
    EXPECT_EQ(0U, stats.messagesPublished.load());
    EXPECT_EQ(0U, stats.publishConfirms.load());
    EXPECT_EQ(0U, stats.publishFailed.load());
    EXPECT_EQ(0U, stats.publishRetries.load());
    EXPECT_EQ(0U, stats.localQueueSize.load());
    EXPECT_EQ(0U, stats.messagesDropped.load());
}

TEST_F(TypesTest, ConsumerStats) {
    ConsumerStats stats;
    
    EXPECT_EQ(0U, stats.messagesReceived.load());
    EXPECT_EQ(0U, stats.messagesAcknowledged.load());
    EXPECT_EQ(0U, stats.messagesRejected.load());
    EXPECT_EQ(0U, stats.messagesRequeued.load());
    EXPECT_EQ(0U, stats.consumerCancellations.load());
    EXPECT_EQ(0U, stats.processingErrors.load());
}

// Exception tests
TEST_F(TypesTest, RabbitMQException) {
    RabbitMQException ex("Test error", ErrorType::NetworkError);
    
    EXPECT_STREQ("Test error", ex.what());
    EXPECT_EQ(ErrorType::NetworkError, ex.getErrorType());
}

TEST_F(TypesTest, SpecificExceptions) {
    ConnectionException connEx("Connection failed");
    EXPECT_EQ(ErrorType::ConnectionError, connEx.getErrorType());
    EXPECT_STREQ("Connection failed", connEx.what());
    
    ChannelException chanEx("Channel error");
    EXPECT_EQ(ErrorType::ChannelError, chanEx.getErrorType());
    
    AuthenticationException authEx("Auth failed");
    EXPECT_EQ(ErrorType::AuthenticationError, authEx.getErrorType());
    
    NetworkException netEx("Network error");
    EXPECT_EQ(ErrorType::NetworkError, netEx.getErrorType());
    
    TimeoutException timeEx("Timeout");
    EXPECT_EQ(ErrorType::TimeoutError, timeEx.getErrorType());
}