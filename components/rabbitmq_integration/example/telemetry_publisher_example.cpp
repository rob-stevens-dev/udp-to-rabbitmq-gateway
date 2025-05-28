// components/rabbitmq-integration/example/telemetry_publisher_example.cpp
#include "rabbitmq_integration/publisher.hpp"
#include "rabbitmq_integration/consumer.hpp"
#include "rabbitmq_integration/queue_manager.hpp"
#include "rabbitmq_integration/message.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace rabbitmq_integration;

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    g_shutdown = true;
}

class TelemetryPublisherExample {
public:
    TelemetryPublisherExample() {
        setupConfiguration();
        setupCallbacks();
    }
    
    bool initialize() {
        std::cout << "Initializing Telemetry Publisher Example..." << std::endl;
        
        // Create queue manager first
        queueManager_ = std::make_unique<QueueManager>(connectionConfig_);
        if (!queueManager_->initialize()) {
            std::cerr << "Failed to initialize queue manager" << std::endl;
            return false;
        }
        
        // Setup default exchanges and validate system
        if (!queueManager_->setupDefaultExchanges()) {
            std::cerr << "Failed to setup default exchanges" << std::endl;
            return false;
        }
        
        // Create publisher
        publisher_ = std::make_unique<Publisher>(publisherConfig_);
        if (!publisher_->start()) {
            std::cerr << "Failed to start publisher" << std::endl;
            return false;
        }
        
        // Create consumer for commands
        consumer_ = std::make_unique<Consumer>(consumerConfig_);
        if (!consumer_->start()) {
            std::cerr << "Failed to start consumer" << std::endl;
            return false;
        }
        
        std::cout << "Initialization complete!" << std::endl;
        return true;
    }
    
    void runExample() {
        std::cout << "Starting telemetry simulation..." << std::endl;
        
        // List of test devices
        std::vector<std::pair<std::string, std::string>> devices = {
            {"na", "123456789012345"},  // North America
            {"eu", "123456789012346"},  // Europe
            {"ap", "123456789012347"}   // Asia Pacific
        };
        
        // Declare queues for all devices
        for (const auto& device : devices) {
            if (!queueManager_->declareDeviceQueues(device.first, device.second)) {
                std::cerr << "Failed to declare queues for device " 
                         << device.first << "." << device.second << std::endl;
                continue;
            }
            
            // Start consuming commands for this device
            startCommandConsumer(device.first, device.second);
        }
        
        // Simulation loop
        int messageCount = 0;
        auto lastStatsTime = std::chrono::steady_clock::now();
        
        while (!g_shutdown) {
            // Send telemetry for each device
            for (const auto& device : devices) {
                if (g_shutdown) break;
                
                sendTelemetryMessage(device.first, device.second);
                messageCount++;
                
                // Occasionally send debug messages
                if (messageCount % 50 == 0) {
                    sendDebugMessage(device.first, device.second, 
                                   "Periodic debug info", 
                                   {{"messageCount", messageCount}});
                }
                
                // Simulate occasional errors
                if (messageCount % 100 == 0) {
                    sendDiscardMessage(device.first, device.second,
                                     "Simulated parsing error",
                                     {0x42, 0x41, 0x44, 0x00}); // "BAD" + null
                }
            }
            
            // Print statistics every 10 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsTime).count() >= 10) {
                printStatistics();
                lastStatsTime = now;
            }
            
            // Wait before next iteration
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Simulation stopped. Total messages sent: " << messageCount << std::endl;
    }
    
    void shutdown() {
        std::cout << "Shutting down..." << std::endl;
        
        if (consumer_) {
            consumer_->stop();
        }
        
        if (publisher_) {
            publisher_->stop();
        }
        
        if (queueManager_) {
            queueManager_->shutdown();
        }
        
        std::cout << "Shutdown complete." << std::endl;
    }

private:
    void setupConfiguration() {
        // Connection configuration
        connectionConfig_.host = "localhost";
        connectionConfig_.port = 5672;
        connectionConfig_.vhost = "/";
        connectionConfig_.username = "guest";
        connectionConfig_.password = "guest";
        connectionConfig_.useTLS = false;
        connectionConfig_.heartbeat = std::chrono::seconds(60);
        connectionConfig_.connectionTimeout = std::chrono::seconds(30);
        connectionConfig_.maxRetries = 3;
        connectionConfig_.retryInterval = std::chrono::milliseconds(1000);
        
        // Publisher configuration
        publisherConfig_ = PublisherConfig(connectionConfig_);
        publisherConfig_.confirmEnabled = true;
        publisherConfig_.persistentMessages = true;
        publisherConfig_.localQueueSize = 10000;
        publisherConfig_.publishTimeout = std::chrono::milliseconds(5000);
        publisherConfig_.batchSize = 10;
        publisherConfig_.batchTimeout = std::chrono::milliseconds(100);
        
        // Consumer configuration
        consumerConfig_ = ConsumerConfig(connectionConfig_);
        consumerConfig_.prefetchCount = 10;
        consumerConfig_.autoAck = false;
        consumerConfig_.ackTimeout = std::chrono::milliseconds(30000);
    }
    
    void setupCallbacks() {
        // Connection callback
        connectionCallback_ = [](ConnectionState state, const std::string& reason) {
            std::cout << "Connection state changed: " 
                     << static_cast<int>(state) << " - " << reason << std::endl;
        };
        
        // Error callback
        errorCallback_ = [](const std::string& error, const std::string& context) {
            std::cerr << "Error in " << context << ": " << error << std::endl;
        };
    }
    
    void sendTelemetryMessage(const std::string& region, const std::string& imei) {
        // Generate simulated telemetry data
        nlohmann::json telemetry = generateTelemetryData();
        
        // Send to inbound queue
        if (!publisher_->publishToInbound(region, imei, telemetry)) {
            std::cerr << "Failed to publish telemetry for " << region << "." << imei << std::endl;
        }
    }
    
    void sendDebugMessage(const std::string& region, const std::string& imei,
                         const std::string& message, const nlohmann::json& context) {
        if (!publisher_->publishToDebug(region, imei, message, context)) {
            std::cerr << "Failed to publish debug message for " << region << "." << imei << std::endl;
        }
    }
    
    void sendDiscardMessage(const std::string& region, const std::string& imei,
                           const std::string& reason, const std::vector<uint8_t>& data) {
        if (!publisher_->publishToDiscard(region, imei, reason, data)) {
            std::cerr << "Failed to publish discard message for " << region << "." << imei << std::endl;
        }
    }
    
    nlohmann::json generateTelemetryData() {
        static int sequence = 0;
        static double latitude = 37.7749;   // San Francisco
        static double longitude = -122.4194;
        static double speed = 0.0;
        static double battery = 1.0;
        
        // Simulate movement
        latitude += (rand() % 21 - 10) * 0.0001;   // ±0.001 degrees
        longitude += (rand() % 21 - 10) * 0.0001;
        speed = 20.0 + (rand() % 41 - 20);         // 0-40 mph
        battery = std::max(0.1, battery - 0.001);   // Slowly drain
        
        nlohmann::json telemetry;
        telemetry["sequence"] = ++sequence;
        telemetry["latitude"] = latitude;
        telemetry["longitude"] = longitude;
        telemetry["speed"] = speed;
        telemetry["battery"] = battery;
        telemetry["satellites"] = 8 + (rand() % 5);
        telemetry["hdop"] = 1.0 + (rand() % 30) * 0.1;
        telemetry["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Add some sensor data
        telemetry["sensors"] = {
            {"temperature", 20.0 + (rand() % 21)},
            {"humidity", 40.0 + (rand() % 41)},
            {"pressure", 1013.25 + (rand() % 21 - 10)}
        };
        
        return telemetry;
    }
    
    void startCommandConsumer(const std::string& region, const std::string& imei) {
        // Create message callback for commands
        MessageCallback callback = [this, region, imei](const std::string& routingKey,
                                                        const Message& message,
                                                        uint64_t deliveryTag) -> bool {
            return handleCommandMessage(region, imei, message, deliveryTag);
        };
        
        // Start consuming from outbound queue
        if (!consumer_->startConsumingFromOutbound(region, imei, callback)) {
            std::cerr << "Failed to start consuming commands for " 
                     << region << "." << imei << std::endl;
        }
    }
    
    bool handleCommandMessage(const std::string& region, const std::string& imei,
                             const Message& message, uint64_t deliveryTag) {
        try {
            std::cout << "Received command for device " << region << "." << imei << std::endl;
            
            // Parse command
            if (!message.isJsonPayload()) {
                std::cerr << "Command message is not JSON" << std::endl;
                return false;
            }
            
            auto commandData = message.getJsonPayload();
            std::string command = commandData.value("command", "unknown");
            
            std::cout << "Processing command: " << command << std::endl;
            std::cout << "Command data: " << commandData.dump(2) << std::endl;
            
            // Simulate command processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Acknowledge successful processing
            if (!consumer_->acknowledge(deliveryTag)) {
                std::cerr << "Failed to acknowledge command message" << std::endl;
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing command: " << e.what() << std::endl;
            
            // Reject and requeue the message
            consumer_->reject(deliveryTag, true);
            return false;
        }
    }
    
    void printStatistics() {
        std::cout << "\n=== Statistics ===" << std::endl;
        
        // Publisher statistics
        auto pubStats = publisher_->getStats();
        std::cout << "Publisher:" << std::endl;
        std::cout << "  Messages published: " << pubStats.messagesPublished << std::endl;
        std::cout << "  Publish confirms: " << pubStats.publishConfirms << std::endl;
        std::cout << "  Publish failed: " << pubStats.publishFailed << std::endl;
        std::cout << "  Local queue size: " << pubStats.localQueueSize << std::endl;
        std::cout << "  Batches published: " << pubStats.batchesPublished << std::endl;
        
        // Consumer statistics  
        auto consStats = consumer_->getStats();
        std::cout << "Consumer:" << std::endl;
        std::cout << "  Messages received: " << consStats.messagesReceived << std::endl;
        std::cout << "  Messages acknowledged: " << consStats.messagesAcknowledged << std::endl;
        std::cout << "  Messages rejected: " << consStats.messagesRejected << std::endl;
        
        // Queue manager statistics
        auto qmStats = queueManager_->getStats();
        std::cout << "Queue Manager:" << std::endl;
        std::cout << "  Queues created: " << qmStats.queuesCreated << std::endl;
        std::cout << "  Operation errors: " << qmStats.operationErrors << std::endl;
        
        std::cout << "==================\n" << std::endl;
    }
    
    // Configuration
    ConnectionConfig connectionConfig_;
    PublisherConfig publisherConfig_;
    ConsumerConfig consumerConfig_;
    
    // Components
    std::unique_ptr<QueueManager> queueManager_;
    std::unique_ptr<Publisher> publisher_;
    std::unique_ptr<Consumer> consumer_;
    
    // Callbacks
    ConnectionCallback connectionCallback_;
    ErrorCallback errorCallback_;
};

// Command-line interface example
class CommandInterface {
public:
    CommandInterface(TelemetryPublisherExample& example) : example_(example) {}
    
    void run() {
        std::string input;
        std::cout << "\nCommands: help, stats, send <region> <imei>, quit" << std::endl;
        std::cout << "> ";
        
        while (std::getline(std::cin, input) && !g_shutdown) {
            processCommand(input);
            if (input == "quit") break;
            std::cout << "> ";
        }
    }

private:
    void processCommand(const std::string& input) {
        std::istringstream iss(input);
        std::string command;
        iss >> command;
        
        if (command == "help") {
            printHelp();
        } else if (command == "stats") {
            // This would call example_.printStatistics() if it was public
            std::cout << "Statistics printed to main output" << std::endl;
        } else if (command == "send") {
            std::string region, imei;
            if (iss >> region >> imei) {
                sendTestMessage(region, imei);
            } else {
                std::cout << "Usage: send <region> <imei>" << std::endl;
            }
        } else if (command == "quit") {
            g_shutdown = true;
        } else if (!command.empty()) {
            std::cout << "Unknown command: " << command << std::endl;
            printHelp();
        }
    }
    
    void printHelp() {
        std::cout << "Available commands:" << std::endl;
        std::cout << "  help                  - Show this help" << std::endl;
        std::cout << "  stats                 - Show statistics" << std::endl;
        std::cout << "  send <region> <imei>  - Send test message" << std::endl;
        std::cout << "  quit                  - Exit application" << std::endl;
    }
    
    void sendTestMessage(const std::string& region, const std::string& imei) {
        std::cout << "Sending test message to " << region << "." << imei << std::endl;
        // This would need access to publisher methods
    }
    
    TelemetryPublisherExample& example_;
};

int main(int argc, char* argv[]) {
    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "RabbitMQ Integration - Telemetry Publisher Example" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    try {
        // Create and initialize example
        TelemetryPublisherExample example;
        if (!example.initialize()) {
            std::cerr << "Failed to initialize example" << std::endl;
            return 1;
        }
        
        // Run in separate thread to allow for interactive commands
        std::thread simulationThread([&example]() {
            example.runExample();
        });
        
        // Run command interface
        CommandInterface cli(example);
        cli.run();
        
        // Wait for simulation to complete
        g_shutdown = true;
        if (simulationThread.joinable()) {
            simulationThread.join();
        }
        
        // Cleanup
        example.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Example completed successfully." << std::endl;
    return 0;
}