# UDP Gateway Component

## Overview

The UDP Gateway component is responsible for handling UDP connections from IoT/mobile devices, processing their messages, and integrating with other system components for message handling.

This component serves as the entry point to the UDP-to-RabbitMQ gateway system, which enables high-performance telemetry collection, command distribution, and data processing for mobile and IoT devices.

## Key Features

- **High Performance**: Built with Boost.Asio for efficient asynchronous I/O
- **IPv4/IPv6 Support**: Handle connections from both IPv4 and IPv6 devices
- **Protocol Agnostic**: Works with any binary protocol through the Protocol Parser component
- **Message Deduplication**: Integrates with Redis for message deduplication
- **RabbitMQ Integration**: Routes messages to appropriate RabbitMQ queues
- **Device Authentication**: Validates devices using IMEI numbers
- **Command Handling**: Delivers pending commands to devices
- **Rate Limiting**: Prevents device message flooding with configurable limits
- **Metrics Collection**: Exposes detailed performance metrics
- **Regional Routing**: Supports data residency requirements via region prefixes

## Dependencies

- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- Boost 1.74+ (system, program_options)
- CMake 3.16+
- Thread library (pthread on Unix-like systems)

### Component Dependencies

The UDP Gateway depends on several other components of the UDP-to-RabbitMQ gateway system:

- **Protocol Parser**: For parsing binary protocols
- **Redis Deduplication**: For preventing duplicate message processing
- **RabbitMQ Integration**: For publishing messages to RabbitMQ queues
- **Device Manager**: For device authentication and command handling
- **Monitoring System**: For metrics collection and reporting

## Building

### Prerequisites

First, ensure you have the necessary tools installed:

#### For Ubuntu/Debian:

```bash
# Install build tools
sudo apt update
sudo apt install -y build-essential cmake ninja-build git

# Install Boost libraries
sudo apt install -y libboost-all-dev
```

#### For macOS:

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build tools
brew install cmake ninja git

# Install Boost
brew install boost
```

#### For Windows:

```powershell
# Using vcpkg (you may need to install vcpkg first)
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install boost:x64-windows
```

### Basic Build

```bash
# Create build directory
mkdir -p build && cd build

# Configure
cmake ../components/udp-gateway

# Build
cmake --build . -j$(nproc)

# Install (optional)
cmake --install . --prefix=/usr/local
```

### Build Options

- `-DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo`: Set build type
- `-DBUILD_TESTING=ON`: Enable unit and integration tests
- `-DBUILD_EXAMPLES=ON`: Build example applications
- `-DCMAKE_CXX_FLAGS="-w"`: Disable warnings (for development only)

## Component Structure

The UDP Gateway is organized into the following key classes:

- **Gateway**: Central component that orchestrates the entire process
- **Session**: Handles individual UDP connections and processes messages
- **RateLimiter**: Provides rate limiting functionality for devices

## Usage

### Running the Server

The UDP Gateway includes a standalone server application:

```bash
# Run with default settings
./udp-gateway-server

# Run with custom settings
./udp-gateway-server --address=0.0.0.0 --port=8125 --region=eu --threads=8 --debug
```

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--help`, `-h` | Show help message | |
| `--address`, `-a` | Listen address | 0.0.0.0 |
| `--port`, `-p` | Listen port | 8125 |
| `--ipv6`, `-6` | Enable IPv6 | false |
| `--region`, `-r` | Region code | na |
| `--threads`, `-t` | Worker threads (0=auto) | 0 |
| `--debug`, `-d` | Enable debugging | false |
| `--ack` | Enable acknowledgments | true |
| `--rate` | Messages per second limit | 10.0 |
| `--burst` | Burst size for rate limiting | 20 |
| `--queue-prefix` | Queue name prefix | "" |
| `--metrics` | Enable metrics | true |
| `--metrics-interval` | Metrics update interval (sec) | 10 |

### Using the Component Library

To use the UDP Gateway as a library in your own applications:

```cpp
#include "udp_gateway/gateway.hpp"
#include <iostream>

int main() {
    // Configure the gateway
    udp_gateway::Gateway::Config config;
    config.listenAddress = "0.0.0.0";
    config.listenPort = 8125;
    config.regionCode = "na";  // North America
    config.numWorkerThreads = 4;
    
    try {
        // Create and start the gateway
        auto gateway = std::make_shared<udp_gateway::Gateway>(config);
        auto result = gateway->start();
        
        if (!result) {
            std::cerr << "Failed to start gateway: " 
                    << udp_gateway::errorCodeToString(result.errorCode) 
                    << " - " << result.errorMessage << std::endl;
            return 1;
        }
        
        std::cout << "Gateway started successfully" << std::endl;
        
        // Wait for termination signal
        waitForTerminationSignal();
        
        // Stop the gateway
        gateway->stop();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

## Docker Deployment

### Building the Docker Image

```bash
# Build the image
docker build -t udp-gateway:latest .

# Run the container
docker run -d -p 8125:8125/udp --name udp-gateway udp-gateway:latest
```

### Docker Compose

Use the provided `docker-compose.yml` file to run the UDP Gateway with its dependencies:

```bash
# Start the entire stack
docker-compose up -d

# View logs
docker-compose logs -f udp-gateway

# Stop the stack
docker-compose down
```

## Development

### Testing

The UDP Gateway includes comprehensive tests:

```bash
# Configure with testing enabled
cd build
cmake ../components/udp-gateway -DBUILD_TESTING=ON

# Build and run tests
cmake --build .
ctest
```

### Mock Components

For testing, the UDP Gateway can use mock implementations of its dependencies:

```cpp
#include "protocol_parser/protocol_parser.hpp"
#include "redis_deduplication/deduplication_service.hpp"
// ... other includes

// Create mock implementations
auto protocolParser = std::make_shared<MockProtocolParser>();
auto deduplicationService = std::make_shared<MockDeduplicationService>();
auto messagePublisher = std::make_shared<MockMessagePublisher>();
auto deviceManager = std::make_shared<MockDeviceManager>();
auto rateLimiter = std::make_shared<MockRateLimiter>();
auto metricsCollector = std::make_shared<MockMetricsCollector>();

// Create gateway with mock components
auto gateway = std::make_shared<udp_gateway::Gateway>(
    config,
    protocolParser,
    deduplicationService,
    messagePublisher,
    deviceManager,
    rateLimiter,
    metricsCollector
);
```

### Debugging

Enable debug mode when starting the gateway:

```bash
./udp-gateway-server --debug
```

This will provide detailed logging of message processing and internal operations.

## Performance Considerations

- The UDP Gateway is designed to handle thousands of concurrent connections
- Worker threads can be adjusted to match available CPU cores
- Rate limiting prevents individual devices from overwhelming the system
- Connection pooling is used for Redis and RabbitMQ integrations
- Metrics collection allows for performance monitoring and tuning

### Scaling Recommendations

- For higher throughput, increase the number of worker threads
- For large deployments, run multiple gateway instances behind a load balancer
- Monitor CPU, memory, and network usage to identify bottlenecks
- Use separate instances for different regions to improve locality

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes
4. Run tests (`cmake --build build --target test`)
5. Commit your changes (`git commit -am 'Add new feature'`)
6. Push to the branch (`git push origin feature/my-feature`)
7. Create a new Pull Request

## License

This component is licensed under the MIT License. See the LICENSE file for details.