/**
 * @file integration_example.cpp
 * @brief Example showing integration with UDP Gateway
 * 
 * This example demonstrates how the Protocol Parser component 
 * can be integrated with the UDP Gateway.
 */

// Using relative paths to help with include path issues
#include "../include/protocol_parser/parser.hpp"
#include "../include/protocol_parser/builder.hpp"
#include "../include/protocol_parser/message.hpp"
#include "../include/protocol_parser/validator.hpp"
#include "../include/protocol_parser/config.hpp"
#include "../include/protocol_parser/protocol.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <thread>

using namespace protocol_parser;

/**
 * @brief Mock UDP Gateway class for demonstration
 */
class MockUdpGateway {
public:
    /**
     * @brief Constructor
     * @param configPath Path to configuration file
     */
    explicit MockUdpGateway(const std::string& configPath = "config/protocol_parser_config.json")
        : parser_({}),
          validator_({}) {
        
        // Register message handlers
        parser_.registerMessageHandler(MessageType::GPS_UPDATE, 
            std::bind(&MockUdpGateway::handleGpsUpdate, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::DEVICE_STATUS, 
            std::bind(&MockUdpGateway::handleDeviceStatus, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::EVENT, 
            std::bind(&MockUdpGateway::handleEvent, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::CMD_ACK, 
            std::bind(&MockUdpGateway::handleCommandAck, this, std::placeholders::_1));
    }
    
    /**
     * @brief Process an incoming UDP packet
     * @param data Binary data
     * @param length Data length
     * @param sourceAddress Source address (for sending responses)
     * @return true if packet was processed successfully, false otherwise
     */
    bool processPacket(const uint8_t* data, size_t length, const std::string& sourceAddress) {
        try {
            // Parse the packet
            Message message = parser_.parse(data, length);
            
            // Store device information
            updateDeviceInfo(message.getImei(), sourceAddress);
            
            // Logging
            std::cout << "Processed packet from " << message.getImei() 
                      << " (seq: " << message.getSequence() << ")"
                      << " type: " << static_cast<int>(message.getType()) << std::endl;
            
            return true;
        } catch (const ProtocolError& e) {
            std::cerr << "Error processing packet: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief Send a command to a device
     * @param imei Device IMEI
     * @param type Command type
     * @param parameters Command parameters
     * @return true if command was sent successfully, false otherwise
     */
    bool sendCommand(const std::string& imei, CommandType type, 
                     const std::map<std::string, Value>& parameters) {
        // Find device address
        auto deviceIt = deviceAddresses_.find(imei);
        if (deviceIt == deviceAddresses_.end()) {
            std::cerr << "Device not found: " << imei << std::endl;
            return false;
        }
        
        // Create command
        CommandBuilder builder = CommandBuilder::create(type);
        builder.setImei(imei)
               .setSequence(nextSequence_++)
               .addParameters(parameters);
        
        // Build command message
        Message message = builder.build();
        
        // Convert to binary
        std::vector<uint8_t> binaryData = message.toBinary();
        
        // Send the binary data (mock implementation)
        std::cout << "Sending command to " << imei 
                  << " at " << deviceIt->second
                  << " (seq: " << message.getSequence() << ")"
                  << " type: " << static_cast<int>(type)
                  << " size: " << binaryData.size() << " bytes" << std::endl;
        
        return true;
    }
    
private:
    /**
     * @brief Load parser configuration from file
     * @param configPath Path to configuration file
     * @return Parser configuration
     */
    static Parser::Config loadParserConfig(const std::string& configPath) {
        try {
            return Config::loadParserConfig(configPath);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load parser configuration: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
            return {};
        }
    }
    
    /**
     * @brief Load validator configuration from file
     * @param configPath Path to configuration file
     * @return Validator configuration
     */
    static Validator::Config loadValidatorConfig(const std::string& configPath) {
        try {
            return Config::loadValidatorConfig(configPath);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load validator configuration: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
            return {};
        }
    }
    
    /**
     * @brief Update device information
     * @param imei Device IMEI
     * @param address Device address
     */
    void updateDeviceInfo(const std::string& imei, const std::string& address) {
        deviceAddresses_[imei] = address;
        deviceLastSeen_[imei] = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Handle GPS update message
     * @param message GPS update message
     */
    void handleGpsUpdate(const Message& message) {
        std::cout << "GPS Update from " << message.getImei() << ": ";
        
        try {
            std::cout << "Lat: " << message.getField<double>("latitude") << ", "
                      << "Lon: " << message.getField<double>("longitude") << ", "
                      << "Alt: " << message.getField<double>("altitude") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[Error reading fields: " << e.what() << "]" << std::endl;
        }
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle device status message
     * @param message Device status message
     */
    void handleDeviceStatus(const Message& message) {
        std::cout << "Device Status from " << message.getImei() << ": ";
        
        try {
            std::cout << "Battery: " << message.getField<float>("batteryLevel") << "%, "
                      << "Signal: " << static_cast<int>(message.getField<uint8_t>("signalStrength")) << ", "
                      << "Charging: " << (message.getField<bool>("charging") ? "Yes" : "No") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[Error reading fields: " << e.what() << "]" << std::endl;
        }
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle event message
     * @param message Event message
     */
    void handleEvent(const Message& message) {
        std::cout << "Event from " << message.getImei() << ": ";
        
        try {
            std::cout << "Type: " << static_cast<int>(message.getField<uint8_t>("eventType")) << ", "
                      << "Time: " << message.getField<uint32_t>("timestamp") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[Error reading fields: " << e.what() << "]" << std::endl;
        }
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle command acknowledgement message
     * @param message Command acknowledgement message
     */
    void handleCommandAck(const Message& message) {
        std::cout << "Command ACK from " << message.getImei() << ": ";
        
        try {
            std::cout << "Command: " << static_cast<int>(message.getField<uint8_t>("commandType")) << ", "
                      << "Sequence: " << message.getField<uint32_t>("originalSequence") << ", "
                      << "Status: " << static_cast<int>(message.getField<uint8_t>("status")) << std::endl;
            
            if (message.hasField("statusMessage")) {
                std::cout << "Status Message: " << message.getField<std::string>("statusMessage") << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "[Error reading fields: " << e.what() << "]" << std::endl;
        }
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    Parser parser_;                                   ///< Protocol parser
    Validator validator_;                             ///< Message validator
    std::unordered_map<std::string, std::string> deviceAddresses_; ///< Device IMEI to address mapping
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> deviceLastSeen_; ///< Device last seen timestamps
    uint32_t nextSequence_ = 1;                       ///< Next sequence number for outgoing commands
};

/**
 * @brief Create a mock GPS update message
 * @param imei Device IMEI
 * @param sequence Sequence number
 * @param latitude Latitude
 * @param longitude Longitude
 * @param altitude Altitude
 * @return Binary message data
 */
std::vector<uint8_t> createMockGpsMessage(const std::string& imei, uint32_t sequence,
                                         double latitude, double longitude, double altitude) {
    Message message(MessageType::GPS_UPDATE, imei, sequence);
    message.addField("latitude", latitude);
    message.addField("longitude", longitude);
    message.addField("altitude", altitude);
    message.addField("accuracy", 5.0f);
    message.addField("speed", 0.0f);
    message.addField("heading", 90.0f);
    
    return message.toBinary();
}

/**
 * @brief Create a mock device status message
 * @param imei Device IMEI
 * @param sequence Sequence number
 * @param batteryLevel Battery level percentage
 * @param signalStrength Signal strength (0-5)
 * @param charging Whether device is charging
 * @return Binary message data
 */
std::vector<uint8_t> createMockStatusMessage(const std::string& imei, uint32_t sequence,
                                           float batteryLevel, uint8_t signalStrength, bool charging) {
    Message message(MessageType::DEVICE_STATUS, imei, sequence);
    message.addField("batteryLevel", batteryLevel);
    message.addField("signalStrength", signalStrength);
    message.addField("charging", charging);
    message.addField("freeMemory", static_cast<uint32_t>(1024 * 1024));
    message.addField("uptime", static_cast<uint64_t>(3600));
    message.addField("temperature", static_cast<uint8_t>(25));
    
    return message.toBinary();
}

/**
 * @brief Main function
 */
int main() {
    std::cout << "Protocol Parser Integration Example" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        // Create mock UDP gateway
        MockUdpGateway gateway;
        
        // Create mock device
        const std::string deviceImei = "123456789012345";
        const std::string deviceAddress = "192.168.1.100:12345";
        uint32_t deviceSequence = 1;
        
        // Simulate receiving GPS update
        std::cout << "\nReceiving GPS update..." << std::endl;
        std::vector<uint8_t> gpsData = createMockGpsMessage(
            deviceImei, deviceSequence++, 37.7749, -122.4194, 10.5);
        
        gateway.processPacket(gpsData.data(), gpsData.size(), deviceAddress);
        
        // Simulate receiving device status
        std::cout << "\nReceiving device status..." << std::endl;
        std::vector<uint8_t> statusData = createMockStatusMessage(
            deviceImei, deviceSequence++, 75.5f, 4, true);
        
        gateway.processPacket(statusData.data(), statusData.size(), deviceAddress);
        
        // Simulate sending reboot command
        std::cout << "\nSending reboot command..." << std::endl;
        gateway.sendCommand(deviceImei, CommandType::REBOOT, {
            {"delay", static_cast<uint16_t>(5)},
            {"reason", std::string("software update")}
        });
        
        // Simulate receiving command acknowledgement
        std::cout << "\nReceiving command acknowledgement..." << std::endl;
        Message ackMessage(MessageType::CMD_ACK, deviceImei, deviceSequence++);
        ackMessage.addField("commandType", static_cast<uint8_t>(static_cast<int>(CommandType::REBOOT)));
        ackMessage.addField("originalSequence", static_cast<uint32_t>(1));
        ackMessage.addField("status", static_cast<uint8_t>(0)); // Success
        ackMessage.addField("statusMessage", std::string("Reboot scheduled"));
        
        std::vector<uint8_t> ackData = ackMessage.toBinary();
        gateway.processPacket(ackData.data(), ackData.size(), deviceAddress);
        
        // Simulate sending configuration command
        std::cout << "\nSending configuration command..." << std::endl;
        std::vector<uint8_t> configData = {0x01, 0x02, 0x03, 0x04};
        gateway.sendCommand(deviceImei, CommandType::CONFIG, {
            {"configType", static_cast<uint8_t>(1)},
            {"configData", configData}
        });
        
        std::cout << "\nExample completed successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}/**
 * @file integration_example.cpp
 * @brief Example showing integration with UDP Gateway
 * 
 * This example demonstrates how the Protocol Parser component 
 * can be integrated with the UDP Gateway.
 */

// Using relative paths to help with include path issues
#include "../include/protocol_parser/parser.hpp"
#include "../include/protocol_parser/builder.hpp"
#include "../include/protocol_parser/message.hpp"
#include "../include/protocol_parser/validator.hpp"
#include "../include/protocol_parser/config.hpp"
#include "../include/protocol_parser/protocol.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <thread>

using namespace protocol_parser;

/**
 * @brief Mock UDP Gateway class for demonstration
 */
class MockUdpGateway {
public:
    /**
     * @brief Constructor
     * @param configPath Path to configuration file
     */
    explicit MockUdpGateway(const std::string& configPath = "config/protocol_parser_config.json")
        : parser_(loadParserConfig(configPath)),
          validator_(loadValidatorConfig(configPath)) {
        
        // Register message handlers
        parser_.registerMessageHandler(MessageType::GPS_UPDATE, 
            std::bind(&MockUdpGateway::handleGpsUpdate, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::DEVICE_STATUS, 
            std::bind(&MockUdpGateway::handleDeviceStatus, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::EVENT, 
            std::bind(&MockUdpGateway::handleEvent, this, std::placeholders::_1));
        
        parser_.registerMessageHandler(MessageType::CMD_ACK, 
            std::bind(&MockUdpGateway::handleCommandAck, this, std::placeholders::_1));
    }
    
    /**
     * @brief Process an incoming UDP packet
     * @param data Binary data
     * @param length Data length
     * @param sourceAddress Source address (for sending responses)
     * @return true if packet was processed successfully, false otherwise
     */
    bool processPacket(const uint8_t* data, size_t length, const std::string& sourceAddress) {
        try {
            // Parse the packet
            Message message = parser_.parse(data, length);
            
            // Store device information
            updateDeviceInfo(message.getImei(), sourceAddress);
            
            // Logging
            std::cout << "Processed packet from " << message.getImei() 
                      << " (seq: " << message.getSequence() << ")"
                      << " type: " << static_cast<int>(message.getType()) << std::endl;
            
            return true;
        } catch (const ProtocolError& e) {
            std::cerr << "Error processing packet: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief Send a command to a device
     * @param imei Device IMEI
     * @param type Command type
     * @param parameters Command parameters
     * @return true if command was sent successfully, false otherwise
     */
    bool sendCommand(const std::string& imei, CommandType type, 
                     const std::map<std::string, Value>& parameters) {
        // Find device address
        auto deviceIt = deviceAddresses_.find(imei);
        if (deviceIt == deviceAddresses_.end()) {
            std::cerr << "Device not found: " << imei << std::endl;
            return false;
        }
        
        // Create command
        CommandBuilder builder = CommandBuilder::create(type);
        builder.setImei(imei)
               .setSequence(nextSequence_++)
               .addParameters(parameters);
        
        // Build command message
        Message message = builder.build();
        
        // Convert to binary
        std::vector<uint8_t> binaryData = message.toBinary();
        
        // Send the binary data (mock implementation)
        std::cout << "Sending command to " << imei 
                  << " at " << deviceIt->second
                  << " (seq: " << message.getSequence() << ")"
                  << " type: " << static_cast<int>(type)
                  << " size: " << binaryData.size() << " bytes" << std::endl;
        
        return true;
    }
    
private:
    /**
     * @brief Load parser configuration from file
     * @param configPath Path to configuration file
     * @return Parser configuration
     */
    static Parser::Config loadParserConfig(const std::string& configPath) {
        try {
            return Config::loadParserConfig(configPath);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load parser configuration: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
            return {};
        }
    }
    
    /**
     * @brief Load validator configuration from file
     * @param configPath Path to configuration file
     * @return Validator configuration
     */
    static Validator::Config loadValidatorConfig(const std::string& configPath) {
        try {
            return Config::loadValidatorConfig(configPath);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load validator configuration: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
            return {};
        }
    }
    
    /**
     * @brief Update device information
     * @param imei Device IMEI
     * @param address Device address
     */
    void updateDeviceInfo(const std::string& imei, const std::string& address) {
        deviceAddresses_[imei] = address;
        deviceLastSeen_[imei] = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Handle GPS update message
     * @param message GPS update message
     */
    void handleGpsUpdate(const Message& message) {
        std::cout << "GPS Update from " << message.getImei() << ": "
                  << "Lat: " << message.getField<double>("latitude") << ", "
                  << "Lon: " << message.getField<double>("longitude") << ", "
                  << "Alt: " << message.getField<double>("altitude") << std::endl;
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle device status message
     * @param message Device status message
     */
    void handleDeviceStatus(const Message& message) {
        std::cout << "Device Status from " << message.getImei() << ": "
                  << "Battery: " << message.getField<float>("batteryLevel") << "%, "
                  << "Signal: " << static_cast<int>(message.getField<uint8_t>("signalStrength")) << ", "
                  << "Charging: " << (message.getField<bool>("charging") ? "Yes" : "No") << std::endl;
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle event message
     * @param message Event message
     */
    void handleEvent(const Message& message) {
        std::cout << "Event from " << message.getImei() << ": "
                  << "Type: " << static_cast<int>(message.getField<uint8_t>("eventType")) << ", "
                  << "Time: " << message.getField<uint32_t>("timestamp") << std::endl;
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    /**
     * @brief Handle command acknowledgement message
     * @param message Command acknowledgement message
     */
    void handleCommandAck(const Message& message) {
        std::cout << "Command ACK from " << message.getImei() << ": "
                  << "Command: " << static_cast<int>(message.getField<uint8_t>("commandType")) << ", "
                  << "Sequence: " << message.getField<uint32_t>("originalSequence") << ", "
                  << "Status: " << static_cast<int>(message.getField<uint8_t>("status")) << std::endl;
        
        if (message.hasField("statusMessage")) {
            std::cout << "Status Message: " << message.getField<std::string>("statusMessage") << std::endl;
        }
        
        // In a real implementation, this would publish to RabbitMQ
        // Example: publishToRabbitMq(message);
    }
    
    Parser parser_;                                   ///< Protocol parser
    Validator validator_;                             ///< Message validator
    std::unordered_map<std::string, std::string> deviceAddresses_; ///< Device IMEI to address mapping
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> deviceLastSeen_; ///< Device last seen timestamps
    uint32_t nextSequence_ = 1;                       ///< Next sequence number for outgoing commands
};

/**
 * @brief Create a mock GPS update message
 * @param imei Device IMEI
 * @param sequence Sequence number
 * @param latitude Latitude
 * @param longitude Longitude
 * @param altitude Altitude
 * @return Binary message data
 */
std::vector<uint8_t> createMockGpsMessage(const std::string& imei, uint32_t sequence,
                                         double latitude, double longitude, double altitude) {
    Message message(MessageType::GPS_UPDATE, imei, sequence);
    message.addField("latitude", latitude);
    message.addField("longitude", longitude);
    message.addField("altitude", altitude);
    message.addField("accuracy", 5.0f);
    message.addField("speed", 0.0f);
    message.addField("heading", 90.0f);
    
    return message.toBinary();
}

/**
 * @brief Create a mock device status message
 * @param imei Device IMEI
 * @param sequence Sequence number
 * @param batteryLevel Battery level percentage
 * @param signalStrength Signal strength (0-5)
 * @param charging Whether device is charging
 * @return Binary message data
 */
std::vector<uint8_t> createMockStatusMessage(const std::string& imei, uint32_t sequence,
                                           float batteryLevel, uint8_t signalStrength, bool charging) {
    Message message(MessageType::DEVICE_STATUS, imei, sequence);
    message.addField("batteryLevel", batteryLevel);
    message.addField("signalStrength", signalStrength);
    message.addField("charging", charging);
    message.addField("freeMemory", static_cast<uint32_t>(1024 * 1024));
    message.addField("uptime", static_cast<uint64_t>(3600));
    message.addField("temperature", static_cast<uint8_t>(25));
    
    return message.toBinary();
}

/**
 * @brief Main function
 */
int main() {
    std::cout << "Protocol Parser Integration Example" << std::endl;
    std::cout << "==================================" << std::endl;
    
    // Create mock UDP gateway
    MockUdpGateway gateway;
    
    // Create mock device
    const std::string deviceImei = "123456789012345";
    const std::string deviceAddress = "192.168.1.100:12345";
    uint32_t deviceSequence = 1;
    
    // Simulate receiving GPS update
    std::cout << "\nReceiving GPS update..." << std::endl;
    std::vector<uint8_t> gpsData = createMockGpsMessage(
        deviceImei, deviceSequence++, 37.7749, -122.4194, 10.5);
    
    gateway.processPacket(gpsData.data(), gpsData.size(), deviceAddress);
    
    // Simulate receiving device status
    std::cout << "\nReceiving device status..." << std::endl;
    std::vector<uint8_t> statusData = createMockStatusMessage(
        deviceImei, deviceSequence++, 75.5f, 4, true);
    
    gateway.processPacket(statusData.data(), statusData.size(), deviceAddress);
    
    // Simulate sending reboot command
    std::cout << "\nSending reboot command..." << std::endl;
    gateway.sendCommand(deviceImei, CommandType::REBOOT, {
        {"delay", static_cast<uint16_t>(5)},
        {"reason", std::string("software update")}
    });
    
    // Simulate receiving command acknowledgement
    std::cout << "\nReceiving command acknowledgement..." << std::endl;
    Message ackMessage(MessageType::CMD_ACK, deviceImei, deviceSequence++);
    ackMessage.addField("commandType", static_cast<uint8_t>(protocol::command_type::REBOOT));
    ackMessage.addField("originalSequence", static_cast<uint32_t>(1));
    ackMessage.addField("status", static_cast<uint8_t>(protocol::status_code::SUCCESS));
    ackMessage.addField("statusMessage", std::string("Reboot scheduled"));
    
    std::vector<uint8_t> ackData = ackMessage.toBinary();
    gateway.processPacket(ackData.data(), ackData.size(), deviceAddress);
    
    // Simulate sending configuration command
    std::cout << "\nSending configuration command..." << std::endl;
    std::vector<uint8_t> configData = {0x01, 0x02, 0x03, 0x04};
    gateway.sendCommand(deviceImei, CommandType::CONFIG, {
        {"configType", static_cast<uint8_t>(1)},
        {"configData", configData}
    });
    
    std::cout << "\nExample completed successfully." << std::endl;
    
    return 0;
}