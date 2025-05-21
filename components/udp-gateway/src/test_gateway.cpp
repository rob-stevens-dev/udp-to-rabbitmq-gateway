// test_gateway.cpp
#include "udp_gateway/gateway.hpp"
#include <iostream>

int main() {
    try {
        // Create gateway configuration
        udp_gateway::Gateway::Config config;
        config.listenAddress = "127.0.0.1";
        config.listenPort = 8125;
        config.regionCode = "na";
        
        std::cout << "Gateway component test - configuration created" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
