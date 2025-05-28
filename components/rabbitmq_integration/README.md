# RabbitMQ Integration Component

High-performance RabbitMQ integration library for the UDP-to-RabbitMQ Gateway system.

## Features

- Reliable message publishing with confirmations
- High-performance message consumption
- Automatic connection recovery
- Local message queuing during outages  
- Comprehensive metrics collection
- Thread-safe design

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

```cpp
#include "rabbitmq_integration/publisher.hpp"
#include "rabbitmq_integration/consumer.hpp"

// Create publisher
PublisherConfig config;
config.host = "localhost";
config.port = 5672;
Publisher publisher(config);

// Publish message
nlohmann::json data = {{"test", "message"}};
publisher.publishToInbound("us-west", "device123", data);
```

## Dependencies

- librabbitmq-c >= 0.11.0
- nlohmann_json >= 3.11.0
- Boost >= 1.74.0
- spdlog (optional)
- fmt (optional)

## License

MIT License - see LICENSE file for details.
