#include "udp_gateway/gateway.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

namespace po = boost::program_options;

// Global signal handler
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Set up command line options
        po::options_description desc("UDP Gateway options");
        desc.add_options()
            ("help,h", "Print help message")
            ("address,a", po::value<std::string>()->default_value("0.0.0.0"), "Listen address")
            ("port,p", po::value<uint16_t>()->default_value(8125), "Listen port")
            ("ipv6,6", po::bool_switch()->default_value(false), "Enable IPv6")
            ("region,r", po::value<std::string>()->default_value("na"), "Region code")
            ("threads,t", po::value<size_t>()->default_value(0), "Number of worker threads (0 = auto)")
            ("debug,d", po::bool_switch()->default_value(false), "Enable debug mode")
            ("ack", po::bool_switch()->default_value(true), "Enable acknowledgments")
            ("rate", po::value<double>()->default_value(10.0), "Default messages per second rate limit")
            ("burst", po::value<size_t>()->default_value(20), "Default burst size for rate limiting")
            ("queue-prefix", po::value<std::string>()->default_value(""), "Queue name prefix")
            ("metrics", po::bool_switch()->default_value(true), "Enable metrics collection")
            ("metrics-interval", po::value<uint32_t>()->default_value(10), "Metrics update interval in seconds");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        
        // Check for help
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        
        // Configure the gateway
        udp_gateway::Gateway::Config config;
        config.listenAddress = vm["address"].as<std::string>();
        config.listenPort = vm["port"].as<uint16_t>();
        config.enableIPv6 = vm["ipv6"].as<bool>();
        config.regionCode = vm["region"].as<std::string>();
        config.numWorkerThreads = vm["threads"].as<size_t>();
        config.enableDebugging = vm["debug"].as<bool>();
        config.enableAcknowledgments = vm["ack"].as<bool>();
        config.defaultMessagesPerSecond = vm["rate"].as<double>();
        config.defaultBurstSize = vm["burst"].as<size_t>();
        config.queuePrefix = vm["queue-prefix"].as<std::string>();
        config.enableMetrics = vm["metrics"].as<bool>();
        config.metricsInterval = std::chrono::seconds(vm["metrics-interval"].as<uint32_t>());
        
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
        
        std::cout << "Gateway started successfully with " << config.numWorkerThreads 
                  << " worker threads" << std::endl;
        
        // Run until signal received
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Periodically print some stats
            static int counter = 0;
            if (++counter % 10 == 0) {  // Every 10 seconds
                auto metrics = gateway.getMetrics();
                std::cout << "Active connections: " << metrics.activeConnections 
                          << ", Processed packets: " << metrics.totalPacketsProcessed 
                          << ", Rejected packets: " << metrics.totalPacketsRejected << std::endl;
            }
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