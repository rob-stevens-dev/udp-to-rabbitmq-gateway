#include <iostream>
#include <vector>
#include <iomanip>

#include "protocol_parser/parser.hpp"
#include "protocol_parser/message.hpp"
#include "protocol_parser/builder.hpp"
#include "protocol_parser/validator.hpp"
#include "protocol_parser/error.hpp"

using namespace protocol_parser;

void printMessage(const Message& message) {
    std::cout << "=== Message Details ===" << std::endl;
    std::cout << "IMEI: " << message.getImei() << std::endl;
    std::cout << "Sequence: " << message.getSequence() << std::endl;
    std::cout << "Type: " << static_cast<int>(message.getType()) << std::endl;
    std::cout << "Valid: " << (message.isValid() ? "Yes" : "No") << std::endl;
    
    if (!message.isValid()) {
        std::cout << "Validation Errors:" << std::endl;
        for (const auto& error : message.getValidationErrors()) {
            std::cout << "  - " << error.message << " (field: " << error.field << ")" << std::endl;
        }
    }
    
    std::cout << "Fields:" << std::endl;
    for (const std::string& fieldName : message.getFieldNames()) {
        std::cout << "  " << fieldName << std::endl;
    }
    std::cout << std::endl;
}

void printBinaryData(const std::vector<uint8_t>& data) {
    std::cout << "Binary data (" << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(data.size(), size_t(16)); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 16) {
        std::cout << "...";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "Protocol Parser Example" << std::endl;
    std::cout << "======================" << std::endl;

    try {
        // Create a parser
        Parser::Config config;
        config.strictMode = true;
        config.validateChecksums = true;
        Parser parser(config);

        // Create a test message
        std::cout << "\n1. Creating a GPS message..." << std::endl;
        Message gpsMessage(MessageType::GPS_UPDATE, "123456789012345", 42);
        
        // Add GPS fields
        gpsMessage.addField("latitude", 37.7749);
        gpsMessage.addField("longitude", -122.4194);
        gpsMessage.addField("altitude", 10.5);
        gpsMessage.addField("speed", 25.5f);
        gpsMessage.addField("heading", 180.0f);
        gpsMessage.addField("satellites", static_cast<uint32_t>(8));

        printMessage(gpsMessage);

        // Convert to binary
        std::cout << "2. Converting to binary..." << std::endl;
        std::vector<uint8_t> binaryData = gpsMessage.toBinary();
        printBinaryData(binaryData);

        // Parse the binary data back
        std::cout << "\n3. Parsing binary data back to message..." << std::endl;
        Message parsedMessage = Message::fromBinary(binaryData);
        printMessage(parsedMessage);

        // Validate the message
        std::cout << "4. Validating the message..." << std::endl;
        Validator validator;
        auto validationErrors = validator.validateMessage(parsedMessage);
        
        if (validationErrors.empty()) {
            std::cout << "✓ Message is valid!" << std::endl;
        } else {
            std::cout << "✗ Message has validation errors:" << std::endl;
            for (const auto& error : validationErrors) {
                std::cout << "  - " << error.message << std::endl;
            }
        }

        // Convert to JSON
        std::cout << "\n5. Converting to JSON..." << std::endl;
        auto jsonData = gpsMessage.toJson();
        std::cout << "JSON: " << jsonData.dump(2) << std::endl;

        // Create a command using the builder
        std::cout << "\n6. Creating a command..." << std::endl;
        auto builder = CommandBuilder::create(CommandType::REBOOT);
        builder.setImei("123456789012345")
               .setSequence(43)
               .addParameter("delay", static_cast<uint16_t>(5))  // Now works with template
               .addParameter("reason", std::string("Maintenance"));

        Message commandMessage = builder.build();
        std::cout << "Command created successfully!" << std::endl;
        std::cout << "  Command Type: " << static_cast<int>(builder.getType()) << std::endl;
        printMessage(commandMessage);

        // Convert command to binary
        std::vector<uint8_t> commandBinary = commandMessage.toBinary();
        printBinaryData(commandBinary);

        // Create a Command object
        Command command(commandMessage);
        std::cout << "Command object details:" << std::endl;
        std::cout << "  Type: " << static_cast<int>(command.getType()) << std::endl;
        std::cout << "  IMEI: " << command.getImei() << std::endl;
        std::cout << "  Sequence: " << command.getSequence() << std::endl;

        // Test with invalid IMEI
        std::cout << "\n7. Testing with invalid IMEI..." << std::endl;
        Message invalidMessage(MessageType::GPS_UPDATE, "12345", 1);  // Too short IMEI
        printMessage(invalidMessage);

        // Test field type conversions
        std::cout << "\n8. Testing field type conversions..." << std::endl;
        Message typeTestMessage(MessageType::STATUS_UPDATE, "123456789012345", 100);
        
        // Add various types
        typeTestMessage.addField("int8_val", static_cast<int8_t>(-42));
        typeTestMessage.addField("uint16_val", static_cast<uint16_t>(1000));
        typeTestMessage.addField("float_val", 3.14159f);
        typeTestMessage.addField("bool_val", true);
        typeTestMessage.addField("string_val", std::string("Hello World"));
        
        std::cout << "Added fields with different types:" << std::endl;
        for (const std::string& fieldName : typeTestMessage.getFieldNames()) {
            std::cout << "  " << fieldName << std::endl;
        }

        // Test type conversions
        try {
            int8_t int8Val = typeTestMessage.getField<int8_t>("int8_val");
            int intVal = typeTestMessage.getField<int>("int8_val");  // Should convert
            std::cout << "int8_val: " << static_cast<int>(int8Val) << " (converted to int: " << intVal << ")" << std::endl;
            
            float floatVal = typeTestMessage.getField<float>("float_val");
            double doubleVal = typeTestMessage.getField<double>("float_val");  // Should convert
            std::cout << "float_val: " << floatVal << " (converted to double: " << doubleVal << ")" << std::endl;
            
            std::string stringVal = typeTestMessage.getField<std::string>("string_val");
            std::cout << "string_val: '" << stringVal << "'" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error accessing fields: " << e.what() << std::endl;
        }

        // Test command builder with different parameter types
        std::cout << "\n9. Testing command builder with various parameter types..." << std::endl;
        auto configBuilder = CommandBuilder::create(CommandType::UPDATE_CONFIG);
        configBuilder.setImei("123456789012345")
                     .setSequence(44)
                     .addParameter("reporting_interval", 30)      // int
                     .addParameter("gps_enabled", true)          // bool
                     .addParameter("max_speed", 120.5f)          // float
                     .addParameter("device_name", std::string("TestDevice"));  // string

        Message configCommand = configBuilder.build();
        std::cout << "Configuration command created:" << std::endl;
        printMessage(configCommand);

        std::cout << "\n✓ All examples completed successfully!" << std::endl;

    } catch (const ParseError& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    } catch (const ProtocolError& e) {
        std::cerr << "Protocol error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}