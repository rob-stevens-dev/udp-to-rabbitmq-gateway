#include "udp_gateway/gateway.hpp"
#include "udp_gateway/rate_limiter.hpp"
#include "mock_interfaces.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace udp_gateway;
using namespace testing;

class GatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.listenAddress = "127.0.0.1";
        config_.listenPort = 8125;
        config_.regionCode = "na";
        config_.numWorkerThreads = 1; // Use minimal threads for testing
        config_.enableDebugging = false;
        config_.enableAcknowledgments = true;
        config_.defaultMessagesPerSecond = 100.0;
        config_.defaultBurstSize = 50;
        
        // Create mock components
        mockProtocolParser_ = std::make_shared<protocol_parser::MockProtocolParser>();
        mockDeduplicationService_ = std::make_shared<redis_deduplication::MockDeduplicationService>();
        mockMessagePublisher_ = std::make_shared<rabbitmq_integration::MockMessagePublisher>();
        mockDeviceManager_ = std::make_shared<device_manager::MockDeviceManager>();
        mockMetricsCollector_ = std::make_shared<monitoring_system::MockMetricsCollector>();
        
        // Create mock rate limiter
        mockRateLimiter_ = std::make_shared<MockRateLimiter>();
    }
    
    class MockRateLimiter : public IRateLimiter {
    public:
        MOCK_METHOD(bool, checkLimit, (const IMEI& deviceId, Priority priority), (override));
        MOCK_METHOD(void, recordMessage, (const IMEI& deviceId, Priority priority, size_t size), (override));
        MOCK_METHOD(void, setDeviceLimit, (const IMEI& deviceId, double messagesPerSecond, size_t burstSize), (override));
        MOCK_METHOD(void, setDefaultLimit, (double messagesPerSecond, size_t burstSize), (override));
        MOCK_METHOD(void, resetDevice, (const IMEI& deviceId), (override));
        MOCK_METHOD((std::unordered_map<std::string, double>), getDeviceStatistics, (const IMEI& deviceId), (const, override));
    };
    
    Gateway::Config config_;
    std::shared_ptr<protocol_parser::MockProtocolParser> mockProtocolParser_;
    std::shared_ptr<redis_deduplication::MockDeduplicationService> mockDeduplicationService_;
    std::shared_ptr<rabbitmq_integration::MockMessagePublisher> mockMessagePublisher_;
    std::shared_ptr<device_manager::MockDeviceManager> mockDeviceManager_;
    std::shared_ptr<monitoring_system::MockMetricsCollector> mockMetricsCollector_;
    std::shared_ptr<MockRateLimiter> mockRateLimiter_;
};

TEST_F(GatewayTest, GatewayConfiguration) {
    // Test that gateway configuration is properly set
    EXPECT_EQ("127.0.0.1", config_.listenAddress);
    EXPECT_EQ(8125, config_.listenPort);
    EXPECT_EQ("na", config_.regionCode);
    EXPECT_EQ(1, config_.numWorkerThreads);
    EXPECT_FALSE(config_.enableDebugging);
    EXPECT_TRUE(config_.enableAcknowledgments);
    EXPECT_EQ(100.0, config_.defaultMessagesPerSecond);
    EXPECT_EQ(50, config_.defaultBurstSize);
}

TEST_F(GatewayTest, GatewayCreationWithCustomComponents) {
    // Should be able to create a gateway with custom components
    EXPECT_NO_THROW({
        auto gateway = std::make_shared<Gateway>(
            config_,
            mockProtocolParser_,
            mockDeduplicationService_,
            mockMessagePublisher_,
            mockDeviceManager_,
            std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
            mockMetricsCollector_
        );
        
        // Gateway should initially not be running
        EXPECT_FALSE(gateway->isRunning());
        
        // Should have zero active connections initially
        EXPECT_EQ(0, gateway->getActiveConnectionCount());
    });
}

TEST_F(GatewayTest, GatewayCreationWithDefaultComponents) {
    // Should be able to create a gateway with default configuration
    EXPECT_NO_THROW({
        Gateway gateway(config_);
        
        // Gateway should initially not be running
        EXPECT_FALSE(gateway.isRunning());
        
        // Should have zero active connections initially
        EXPECT_EQ(0, gateway.getActiveConnectionCount());
    });
}

TEST_F(GatewayTest, GatewayMetrics) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    // Get initial metrics
    auto metrics = gateway->getMetrics();
    
    // Should have initial values
    EXPECT_EQ(0, metrics.totalPacketsReceived);
    EXPECT_EQ(0, metrics.totalPacketsProcessed);
    EXPECT_EQ(0, metrics.totalPacketsRejected);
    EXPECT_EQ(0, metrics.totalPacketsDuplicated);
    EXPECT_EQ(0, metrics.totalBytesReceived);
    EXPECT_EQ(0, metrics.activeConnections);
    EXPECT_EQ(0, metrics.peakConnections);
}

TEST_F(GatewayTest, HandleMessage) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    // Create a test message
    DeviceMessage message;
    message.deviceId = "123456789012345";
    message.region = "na";
    message.payload = {0x01, 0x02, 0x03};
    message.messageId = "test-msg-123";
    
    // Set up expectations
    EXPECT_CALL(*mockMessagePublisher_, publishMessage(_, _))
        .WillOnce(Return(true));
    
    EXPECT_CALL(*mockMetricsCollector_, incrementCounter(_, _))
        .Times(AtLeast(1));
    
    // Handle the message
    auto result = gateway->handleMessage(message);
    
    // Should succeed
    EXPECT_TRUE(result);
}

TEST_F(GatewayTest, HandleMessagePublishFailure) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    // Create a test message
    DeviceMessage message;
    message.deviceId = "123456789012345";
    message.region = "na";
    message.payload = {0x01, 0x02, 0x03};
    message.messageId = "test-msg-123";
    
    // Set up expectations for publish failure
    EXPECT_CALL(*mockMessagePublisher_, publishMessage(_, _))
        .WillOnce(Return(false));
    
    // Handle the message
    auto result = gateway->handleMessage(message);
    
    // Should fail
    EXPECT_FALSE(result);
    EXPECT_EQ(ErrorCode::DEPENDENCY_FAILURE, result.errorCode);
}

TEST_F(GatewayTest, HandleError) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    // Set up expectations
    EXPECT_CALL(*mockMetricsCollector_, recordError(_))
        .Times(1);
    
    // For INVALID_MESSAGE errors, the gateway tries to publish to discard queue
    EXPECT_CALL(*mockMessagePublisher_, publishMessage(_, _))
        .WillOnce(Return(true));
    
    // Should not throw when handling errors
    EXPECT_NO_THROW({
        gateway->handleError("123456789012345", ErrorCode::INVALID_MESSAGE, "Test error");
    });
}

TEST_F(GatewayTest, HandleErrorNoDiscard) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    // Set up expectations - only metrics should be called for non-discard errors
    EXPECT_CALL(*mockMetricsCollector_, recordError(_))
        .Times(1);
    
    // No message publishing should happen for rate limit errors
    EXPECT_CALL(*mockMessagePublisher_, publishMessage(_, _))
        .Times(0);
    
    // Should not throw when handling errors
    EXPECT_NO_THROW({
        gateway->handleError("123456789012345", ErrorCode::RATE_LIMITED, "Test rate limit error");
    });
}

TEST_F(GatewayTest, GetPendingCommands) {
    auto gateway = std::make_shared<Gateway>(
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        std::static_pointer_cast<IRateLimiter>(mockRateLimiter_),
        mockMetricsCollector_
    );
    
    std::vector<DeviceCommand> expectedCommands;
    DeviceCommand cmd;
    cmd.deviceId = "123456789012345";
    cmd.commandId = "cmd-123";
    expectedCommands.push_back(cmd);
    
    // Set up expectations
    EXPECT_CALL(*mockDeviceManager_, getPendingCommands("123456789012345"))
        .WillOnce(Return(expectedCommands));
    
    // Get pending commands
    auto commands = gateway->getPendingCommands("123456789012345");
    
    // Should return the expected commands
    EXPECT_EQ(1, commands.size());
    EXPECT_EQ("cmd-123", commands[0].commandId);
}

// Note: Testing gateway start/stop is complex because it involves network operations
// and would require more sophisticated mocking of the network stack.
// For comprehensive integration testing, these would be tested separately.