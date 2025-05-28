#include "rabbitmq_integration/types.hpp"
#include <iostream>

int main() {
    using namespace rabbitmq_integration;
    
    // Test utility functions
    std::string queueName = utils::queueNameForDevice("us-west", "device123", QueueType::Inbound);
    std::string exchangeName = utils::exchangeNameForType(QueueType::Inbound);
    
    std::cout << "Queue name: " << queueName << std::endl;
    std::cout << "Exchange name: " << exchangeName << std::endl;
    
    return 0;
}
