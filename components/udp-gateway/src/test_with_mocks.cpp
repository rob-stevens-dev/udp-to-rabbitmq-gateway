// test_with_mocks.cpp
#include "udp_gateway/gateway.hpp"
#include "udp_gateway/session.hpp"
#include "udp_gateway/rate_limiter.hpp"
#include <iostream>
#include <memory>

// Mock implementations for dependencies
class MockProtocolParser : public protocol_parser::IProtocolParser {
public:
    udp_gateway::Result<udp_gateway::DeviceMessage> parseMessage(const std::vector<uint8_t>& data) override {
        // Simple mock implementation
        udp_gateway::DeviceMessage message;
        message.deviceId = "123456789012345";
        return udp_gateway::Result<udp_gateway::DeviceMessage>::ok(message);
    }
    
    std::vector<uint8_t> createAcknowledgment(const udp_gateway::MessageAcknowledgment& ack) override {
        return {0x01, 0x02, 0x03}; // Simple mock response
    }
    
    std::vector<uint8_t> createCommandPacket(const std::vector<udp_gateway::DeviceCommand>& commands) override {
        return {0x04, 0x05, 0x06}; // Simple mock response
    }
};

// Add other mock implementations similarly...

int main() {
    try {
        // Create a shared gateway configuration
        auto rateLimiter = udp_gateway::createDefaultRateLimiter();
        auto protocolParser = std::make_shared<MockProtocolParser>();
        
        std::cout << "Mock components created successfully" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
