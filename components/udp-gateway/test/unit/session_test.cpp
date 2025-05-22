#include "udp_gateway/session.hpp"
#include "udp_gateway/rate_limiter.hpp"
#include "mock_interfaces.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio.hpp>

using namespace udp_gateway;
using namespace testing;

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.regionCode = "na";
        config_.maxMessageSize = 8192;
        config_.enableDebugging = false;
        config_.enableAcknowledgments = true;
        
        // Create mock components
        mockProtocolParser_ = std::make_shared<protocol_parser::MockProtocolParser>();
        mockDeduplicationService_ = std::make_shared<redis_deduplication::MockDeduplicationService>();
        mockMessagePublisher_ = std::make_shared<rabbitmq_integration::MockMessagePublisher>();
        mockDeviceManager_ = std::make_shared<device_manager::MockDeviceManager>();
        mockMetricsCollector_ = std::make_shared<monitoring_system::MockMetricsCollector>();
        mockSessionHandler_ = std::make_shared<MockSessionHandler>();
        
        // Create rate limiter
        TokenBucketRateLimiter::Config rateLimiterConfig;
        rateLimiterConfig.defaultMessagesPerSecond = 100.0; // High limit for testing
        rateLimiterConfig.defaultBurstSize = 100;
        rateLimiter_ = std::make_shared<TokenBucketRateLimiter>(rateLimiterConfig);
    }
    
    class MockSessionHandler : public ISessionHandler {
    public:
        MOCK_METHOD(VoidResult, handleMessage, (const DeviceMessage& message), (override));
        MOCK_METHOD(void, handleError, (const IMEI& deviceId, ErrorCode errorCode, const std::string& errorMessage), (override));
        MOCK_METHOD((std::vector<DeviceCommand>), getPendingCommands, (const IMEI& deviceId), (override));
    };
    
    boost::asio::io_context ioContext_;
    Session::Config config_;
    std::shared_ptr<protocol_parser::MockProtocolParser> mockProtocolParser_;
    std::shared_ptr<redis_deduplication::MockDeduplicationService> mockDeduplicationService_;
    std::shared_ptr<rabbitmq_integration::MockMessagePublisher> mockMessagePublisher_;
    std::shared_ptr<device_manager::MockDeviceManager> mockDeviceManager_;
    std::shared_ptr<monitoring_system::MockMetricsCollector> mockMetricsCollector_;
    std::shared_ptr<MockSessionHandler> mockSessionHandler_;
    std::shared_ptr<IRateLimiter> rateLimiter_;
};

TEST_F(SessionTest, SessionConfiguration) {
    // Test that session configuration is properly set
    EXPECT_EQ("na", config_.regionCode);
    EXPECT_EQ(8192, config_.maxMessageSize);
    EXPECT_FALSE(config_.enableDebugging);
    EXPECT_TRUE(config_.enableAcknowledgments);
}

TEST_F(SessionTest, SessionCreation) {
    // Create a UDP socket for testing
    boost::asio::ip::udp::socket socket(ioContext_);
    
    // Should be able to create a session without throwing
    EXPECT_NO_THROW({
        auto session = std::make_shared<Session>(
            ioContext_,
            std::move(socket),
            config_,
            mockProtocolParser_,
            mockDeduplicationService_,
            mockMessagePublisher_,
            mockDeviceManager_,
            rateLimiter_,
            mockMetricsCollector_,
            mockSessionHandler_
        );
        
        // Session should initially not be active
        EXPECT_FALSE(session->isActive());
    });
}

TEST_F(SessionTest, SessionStartStop) {
    // Create a UDP socket for testing
    boost::asio::ip::udp::socket socket(ioContext_);
    
    auto session = std::make_shared<Session>(
        ioContext_,
        std::move(socket),
        config_,
        mockProtocolParser_,
        mockDeduplicationService_,
        mockMessagePublisher_,
        mockDeviceManager_,
        rateLimiter_,
        mockMetricsCollector_,
        mockSessionHandler_
    );
    
    // Start the session
    session->start();
    EXPECT_TRUE(session->isActive());
    
    // Stop the session
    session->stop();
    EXPECT_FALSE(session->isActive());
}

// Note: More comprehensive session tests would require setting up actual UDP sockets
// and network operations, which is complex in a unit test environment.
// For now, we test the basic lifecycle and configuration.