# Protocol Parser Component

## Overview

The Protocol Parser component is responsible for parsing, validating, and constructing binary protocol messages used in communication between mobile/IoT devices and the UDP Gateway system. It provides a C++ API for handling binary data packets, extracting telemetry data, and constructing command messages.

## Features

- **Binary Protocol Parsing**: Efficiently parse binary UDP packets from devices with zero-copy support
- **Message Validation**: Validate message structure, check checksums, and verify field values
- **Command Construction**: Create outbound command messages for device configuration and control
- **Multiple Telemetry Types**: Support for GPS coordinates, device status, events, and more
- **Protocol Versioning**: Handle multiple protocol versions with backward compatibility
- **Field Type Safety**: Strong type checking for message fields
- **Extensible Design**: Support for adding new message types and fields
- **Minimal Dependencies**: Only requires Boost and nlohmann/json
- **Performance Optimized**: Designed for high-throughput processing

## Architecture

The component implements a clean separation of concerns through several key classes:

- **Parser**: Main entry point for parsing binary data into messages
- **Message**: Represents a parsed message with typed field access
- **Validator**: Validates messages against protocol rules
- **CommandBuilder**: Constructs command messages using a fluent interface
- **Error System**: Provides detailed error information for debugging

## Message Types

The component supports several message types:

### Inbound (Device to Server)
- **GPS Updates**: Location information with coordinates, altitude, speed, etc.
- **Device Status**: Battery level, signal strength, memory usage, etc.
- **Events**: Various device events and notifications
- **Command Acknowledgements**: Responses to previously sent commands

### Outbound (Server to Device)
- **Subscribe**: Subscribe to data feeds
- **Unsubscribe**: Unsubscribe from data feeds
- **Reboot**: Trigger device reboot
- **Configuration**: Update device configuration

## Usage Examples

### Parsing a Binary Message

```cpp
#include "protocol_parser/parser.hpp"
#include "protocol_parser/message.hpp"

// Create a parser
protocol_parser::Parser parser;

// Parse a binary packet
try {
    std::vector<uint8_t> binaryData = receiveUdpPacket();
    protocol_parser::Message message = parser.parse(binaryData);
    
    // Access parsed fields
    std::string imei = message.getImei();
    uint32_t sequence = message.getSequence();
    
    // Handle different message types
    if (message.getType() == protocol_parser::MessageType::GPS_UPDATE) {
        double latitude = message.getField<double>("latitude");
        double longitude = message.getField<double>("longitude");
        double altitude = message.getField<double>("altitude");
        
        // Process GPS data...
    }
} catch (const protocol_parser::ProtocolError& e) {
    // Handle parsing error
    std::cerr << "Parse error: " << e.what() << std::endl;
}
```

### Creating a Command Message

```cpp
#include "protocol_parser/builder.hpp"

// Create a command builder
protocol_parser::CommandBuilder builder = 
    protocol_parser::CommandBuilder::create(protocol_parser::CommandType::REBOOT);

// Configure the command
builder.setImei("123456789012345")
       .setSequence(42)
       .addParameter("delay", 5);  // Reboot after 5 seconds

// Build the command message
protocol_parser::Message command = builder.build();

// Convert to binary for transmission
std::vector<uint8_t> binaryData = command.toBinary();

// Send the binary data
sendUdpPacket(binaryData);
```

### Validating a Message

```cpp
#include "protocol_parser/validator.hpp"

// Configure a validator
protocol_parser::Validator::Config config;
config.strictMode = true;
config.validateChecksum = true;

protocol_parser::Validator validator(config);

// Validate a message
if (!validator.validate(message)) {
    // Handle validation failures
    const auto& errors = message.getValidationErrors();
    for (const auto& error : errors) {
        std::cerr << "Validation error: " << error.message << std::endl;
    }
}
```

## Building the Component

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- Boost libraries (for endian handling, etc.)
- nlohmann/json library

### Build Instructions

```bash
# From the monorepo root
mkdir -p build && cd build

# Configure
cmake ..

# Build just the protocol parser
cmake --build . --target protocol_parser

# Run tests (if enabled)
ctest -R protocol_parser_.*
```

### Linking with Your Project

```cmake
# In your CMakeLists.txt
find_package(protocol_parser REQUIRED)
target_link_libraries(your_target PRIVATE protocol_parser::protocol_parser)
```

## Extending the Component

### Adding a New Message Type

1. Add a new enum value in `MessageType` (in `types.hpp`)
2. Implement parsing logic in `Parser::parsePayload()`
3. Add validation rules in `Validator::validateMessageFields()`
4. Implement serialization in `Message::toBinary()`

### Adding a Custom Validator

```cpp
// Register a custom field validator
validator.registerFieldValidator(
    protocol_parser::MessageType::GPS_UPDATE,
    "accuracy",
    [](const std::string& fieldName, const protocol_parser::Value& value, 
       protocol_parser::ErrorInfo& error) {
        try {
            float accuracy = std::get<float>(value);
            if (accuracy > 100.0f) {
                error = {
                    protocol_parser::ErrorCategory::VALIDATION_ERROR,
                    "Accuracy too high",
                    fieldName,
                    0,
                    0,
                    0
                };
                return false;
            }
            return true;
        } catch (...) {
            return false;
        }
    }
);
```

## Performance Considerations

- Use zero-copy parsing for large packets by using the `Parser::parse(const uint8_t*, size_t)` overload
- Configure the parser memory allocation strategy based on your usage pattern
- Use `tryParse()` for non-throwing error handling in performance-critical paths
- Consider registering message handlers to avoid copying data in high-throughput scenarios

## Thread Safety

The component is designed with these thread safety guarantees:

- **Thread Compatible**: Different instances can be used from different threads simultaneously
- **Not Thread Safe**: Single instances should not be used from multiple threads concurrently without external synchronization
- **Reentrant**: Most methods are reentrant, meaning they can be safely interrupted and called again

## Further Reading

- See `example/` directory for more usage examples
- Refer to API documentation for detailed information on each class and method
- For protocol specification details, contact the system architecture team