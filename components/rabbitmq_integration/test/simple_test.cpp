// components/rabbitmq-integration/test/simple_test.cpp
// Simple test without external dependencies
#include "rabbitmq_integration/types.hpp"
#include "rabbitmq_integration/message.hpp"
#include <iostream>
#include <cassert>

using namespace rabbitmq_integration;

void test_queue_naming() {
    std::cout << "Testing queue naming..." << std::endl;
    
    std::string result = utils::queueNameForDevice("us-west", "123456789", QueueType::Inbound);
    assert(result == "us-west.123456789_inbound");
    
    result = utils::queueNameForDevice("eu-central", "987654321", QueueType::Debug);
    assert(result == "eu-central.987654321_debug");
    
    std::cout << "Queue naming tests passed!" << std::endl;
}

void test_exchange_naming() {
    std::cout << "Testing exchange naming..." << std::endl;
    
    std::string result = utils::exchangeNameForType(QueueType::Inbound);
    assert(result == "telemetry.exchange");
    
    result = utils::exchangeNameForType(QueueType::Outbound);
    assert(result == "command.exchange");
    
    std::cout << "Exchange naming tests passed!" << std::endl;
}

void test_message_creation() {
    std::cout << "Testing message creation..." << std::endl;
    
    // Test JSON message
    nlohmann::json testData = {
        {"test", "value"},
        {"number", 42}
    };
    
    Message msg(testData);
    assert(msg.isJsonPayload());
    assert(!msg.isBinaryPayload());
    assert(!msg.isStringPayload());
    
    auto retrievedJson = msg.getJsonPayload();
    assert(retrievedJson["test"] == "value");
    assert(retrievedJson["number"] == 42);
    
    // Test string message
    std::string testString = "Hello, RabbitMQ!";
    Message stringMsg(testString);
    assert(!stringMsg.isJsonPayload());
    assert(!stringMsg.isBinaryPayload());
    assert(stringMsg.isStringPayload());
    
    assert(stringMsg.getStringPayload() == testString);
    
    std::cout << "Message creation tests passed!" << std::endl;
}

void test_message_properties() {
    std::cout << "Testing message properties..." << std::endl;
    
    Message msg(nlohmann::json{{"test", "properties"}});
    
    msg.setMessageId("test-message-123");
    msg.setCorrelationId("correlation-456");
    msg.setPriority(MessagePriority::High);
    msg.setPersistent(true);
    
    assert(msg.getMessageId().value() == "test-message-123");
    assert(msg.getCorrelationId().value() == "correlation-456");
    assert(msg.getPriority() == MessagePriority::High);
    assert(msg.isPersistent() == true);
    
    std::cout << "Message properties tests passed!" << std::endl;
}

void test_message_headers() {
    std::cout << "Testing message headers..." << std::endl;
    
    Message msg(nlohmann::json{{"test", "headers"}});
    
    msg.setHeader("region", "us-west");
    msg.setHeader("device-id", "device123");
    msg.setHeader("priority", "high");
    
    assert(msg.getHeader("region").value() == "us-west");
    assert(msg.getHeader("device-id").value() == "device123");
    assert(msg.getHeader("priority").value() == "high");
    assert(!msg.getHeader("non-existent").has_value());
    
    auto headers = msg.getHeaders();
    assert(headers.size() == 3);
    assert(headers.at("region") == "us-west");
    
    std::cout << "Message headers tests passed!" << std::endl;
}

void test_factory_methods() {
    std::cout << "Testing factory methods..." << std::endl;
    
    // Test telemetry message creation
    nlohmann::json telemetryData = {
        {"latitude", 37.7749},
        {"longitude", -122.4194},
        {"speed", 35.0}
    };
    
    auto telemetryMsg = Message::createTelemetryMessage("us-west", "device123", telemetryData);
    assert(telemetryMsg.isJsonPayload());
    assert(telemetryMsg.getType().value() == "telemetry");
    assert(telemetryMsg.getHeader("region").value() == "us-west");
    assert(telemetryMsg.getHeader("imei").value() == "device123");
    
    // Test command message creation
    nlohmann::json commandParams = {
        {"interval", 30},
        {"mode", "continuous"}
    };
    
    auto commandMsg = Message::createCommandMessage("eu-central", "device456", "SET_INTERVAL", commandParams);
    assert(commandMsg.isJsonPayload());
    assert(commandMsg.getType().value() == "command");
    assert(commandMsg.getPriority() == MessagePriority::High);
    assert(commandMsg.getHeader("region").value() == "eu-central");
    assert(commandMsg.getHeader("command").value() == "SET_INTERVAL");
    
    std::cout << "Factory methods tests passed!" << std::endl;
}

void test_timestamp_utilities() {
    std::cout << "Testing timestamp utilities..." << std::endl;
    
    auto now = std::chrono::system_clock::now();
    std::string timestampStr = utils::timestampToString(now);
    
    assert(!timestampStr.empty());
    assert(timestampStr.find('T') != std::string::npos); // ISO format
    assert(timestampStr.find('Z') != std::string::npos); // UTC indicator
    
    auto parsedTime = utils::stringToTimestamp(timestampStr);
    
    // Allow for small differences due to precision
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::abs(now - parsedTime));
    assert(diff.count() < 1000); // Less than 1 second difference
    
    std::cout << "Timestamp utilities tests passed!" << std::endl;
}

int main() {
    std::cout << "Running RabbitMQ Integration Simple Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        test_queue_naming();
        test_exchange_naming();
        test_message_creation();
        test_message_properties();
        test_message_headers();
        test_factory_methods();
        test_timestamp_utilities();
        
        std::cout << std::endl;
        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}