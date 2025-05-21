#include "udp_gateway/gateway.hpp"
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

// Global signal handler
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Configure the gateway
        udp_gateway::Gateway::Config config;
        config.listenAddress = "0.0.0.0";
        config.listenPort = 8125;
        config.regionCode = "na";  // North America
        config.numWorkerThreads = 4;
        
        std::cout << "Starting UDP Gateway on " << config.listenAddress 
                  << ":" << config.listenPort 
                  << " (Region: " << config.regionCode << ")" << std::endl;
        
        // Create and start the gateway
        udp_gateway::Gateway gateway(config);
        auto result = gateway.start();
        
        if (!result) {
            std::cerr << "Failed to start gateway: " 
                      << udp_gateway::errorCodeToString(result.errorCode) 
                      << " - " << result.errorMessage << std::endl;
            return 1;
        }
        
        std::cout << "Gateway started successfully" << std::endl;
        
        // Run until signal received
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Stop the gateway
        std::cout << "Stopping the gateway..." << std::endl;
        gateway.stop();
        std::cout << "Gateway stopped" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}