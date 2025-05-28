#include "rabbitmq_integration/message.hpp"
#include <iostream>

int main() {
    using namespace rabbitmq_integration;
    
    // Create a simple message
    nlohmann::json data = {
        {"message", "Hello from simple publisher!"},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    
    Message msg(data);
    msg.setMessageId("simple-test-123");
    
    std::cout << "Created message: " << msg.toString() << std::endl;
    std::cout << "Message size: " << msg.getPayloadSize() << " bytes" << std::endl;
    
    return 0;
}
