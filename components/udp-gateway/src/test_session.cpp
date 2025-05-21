// test_session.cpp
#include "udp_gateway/session.hpp"
#include <iostream>
#include <boost/asio.hpp>

int main() {
    try {
        // Create io_context
        boost::asio::io_context io_context;
        
        // Create socket (will not actually be used)
        boost::asio::ip::udp::socket socket(io_context);
        
        // Create session configuration
        udp_gateway::Session::Config config;
        config.regionCode = "na";
        
        std::cout << "Session component test - configuration created" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
