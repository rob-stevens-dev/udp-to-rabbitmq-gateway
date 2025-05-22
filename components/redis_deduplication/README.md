# Redis Deduplication Component

A high-performance, thread-safe C++ library for message deduplication using Redis as the backend store with local cache fallback support.

## Overview

The Redis Deduplication component is part of a larger UDP-to-RabbitMQ gateway system. It provides efficient duplicate message detection using Redis SET NX operations with configurable time-to-live (TTL) windows, backed by a local LRU cache for improved performance and resilience.

## Features

- **High Performance**: Optimized for thousands of deduplication checks per second
- **Thread-Safe**: Fully thread-safe implementation suitable for concurrent applications
- **Redis Integration**: Uses Redis SET NX for atomic duplicate checking
- **Local Cache Fallback**: LRU cache provides resilience when Redis is unavailable
- **Connection Pooling**: Efficient connection management with automatic failover
- **Batch Operations**: Support for batch deduplication to reduce network overhead
- **Health Monitoring**: Built-in health checks and connection monitoring
- **Master/Standby Support**: Automatic failover between Redis master and standby instances
- **Comprehensive Metrics**: Detailed statistics and performance monitoring
- **Modern C++**: Built with C++17/20 using best practices

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │  Deduplicator   │    │ Connection Pool │
│                 │───▶│                 │───▶│                 │
│                 │    │  ┌──────────────┐│    │ ┌─────────────┐ │
│                 │    │  │ Local Cache  ││    │ │Redis Master │ │
│                 │    │  │     (LRU)    ││    │ │             │ │
│                 │    │  └──────────────┘│    │ └─────────────┘ │
│                 │    │                 │    │ ┌─────────────┐ │
│                 │    │                 │    │ │Redis Standby│ │
│                 │    │                 │    │ │             │ │
└─────────────────┘    └─────────────────┘    │ └─────────────┘ │
                                              └─────────────────┘
```

## Quick Start

### Dependencies

- **C++ Compiler**: GCC 9+ or Clang 10+ with C++17 support
- **CMake**: 3.16 or later
- **hiredis**: Redis C client library
- **Boost**: System and Thread libraries
- **spdlog**: Fast C++ logging library

### Building

```bash
# Clone and navigate to component directory
cd components/redis-deduplication

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest
```

### Basic Usage

```cpp
#include "redis_deduplication/deduplicator.hpp"

using namespace redis_deduplication;

int main() {
    // Create configuration
    auto config = createDevelopmentConfig("localhost", 6379);
    config.password = "your-redis-password";
    
    // Create and initialize deduplicator
    Deduplicator deduplicator(config);
    if (!deduplicator.initialize()) {
        return 1;
    }
    
    // Check if message is duplicate
    std::string imei = "123456789012345";
    uint32_t sequenceNumber = 42;
    
    auto result = deduplicator.isDuplicate(imei, sequenceNumber);
    
    if (result == DeduplicationResult::UNIQUE) {
        std::cout << "Message is unique, processing..." << std::endl;
        // Process the message
    } else if (result == DeduplicationResult::DUPLICATE) {
        std::cout << "Message is duplicate, ignoring..." << std::endl;
    } else {
        std::cout << "Error checking message" << std::endl;
    }
    
    return 0;
}
```

## Configuration

### Development Configuration

```cpp
auto config = createDevelopmentConfig("localhost", 6379);
// Optimized for development with debug logging enabled
```

### Production Configuration

```cpp
auto config = createProductionConfig("redis-master.example.com", 6379, "secure-password");
config.standbyHost = "redis-standby.example.com";
config.standbyPort = 6379;
// Optimized for production with larger cache and connection pool
```

### Custom Configuration

```cpp
DeduplicationConfig config;
config.masterHost = "redis.example.com";
config.masterPort = 6379;
config.password = "password";
config.connectionPoolSize = 10;
config.deduplicationWindow = std::chrono::minutes(30);
config.localCacheSize = 50000;
config.enableLocalCacheFallback = true;
```

## Advanced Features

### Batch Operations

```cpp
std::vector<std::pair<std::string, uint32_t>> messages = {
    {"123456789012345", 100},
    {"123456789012345", 101},
    {"987654321098765", 200}
};

auto batchResult = deduplicator.areDuplicates(messages);

for (size_t i = 0; i < messages.size(); ++i) {
    if (batchResult.results[i] == DeduplicationResult::UNIQUE) {
        // Process unique message
    }
}
```

### Statistics and Monitoring

```cpp
auto stats = deduplicator.getStats();
std::cout << "Total checks: " << stats.totalChecks << std::endl;
std::cout << "Duplicates found: " << stats.duplicatesFound << std::endl;
std::cout << "Average latency: " << stats.averageCheckLatency.count() << "μs" << std::endl;
std::cout << "Cache hit rate: " << 
    (double)stats.localCacheHits / (stats.localCacheHits + stats.localCacheMisses) << std::endl;
```

### Health Monitoring

```cpp
if (!deduplicator.healthCheck()) {
    std::cerr << "Deduplication system is unhealthy!" << std::endl;
}

if (deduplicator.isUsingStandby()) {
    std::cout << "Currently using standby Redis instance" << std::endl;
}
```

## Docker Development Environment

### Quick Start with Docker Compose

```bash
# Start Redis and development environment
docker-compose up -d

# Access development container
docker-compose exec redis-dedup-dev bash

# Build and test
cd /workspace
mkdir -p build && cd build
cmake ..
cmake --build .
ctest

# Run example
./example/simple_deduplication redis-master 6379 dev-password
```

### Available Services

- **redis-master**: Primary Redis instance (port 6379)
- **redis-standby**: Standby Redis instance (port 6380)
- **redis-sentinel**: Redis Sentinel for automatic failover (port 26379)
- **redis-dedup-dev**: Development container with build tools
- **dedup-example**: Example application container

### Monitoring Stack (Optional)

```bash
# Start with monitoring
docker-compose --profile monitoring up -d

# Access services
# - RedisInsight: http://localhost:8001
# - Prometheus: http://localhost:9090
# - Grafana: http://localhost:3000 (admin/admin)
```

## Performance

### Benchmarks

- **Throughput**: 10,000+ deduplication checks per second
- **Latency**: Sub-5ms average latency for Redis operations
- **Cache Performance**: Sub-100μs for local cache hits
- **Memory Usage**: Configurable cache size with LRU eviction

### Optimization Tips

1. **Batch Operations**: Use batch checking for multiple messages
2. **Connection Pooling**: Tune pool size based on concurrency needs
3. **Local Cache**: Enable local cache for better performance and resilience
4. **Deduplication Window**: Balance between accuracy and memory usage
5. **Pipelining**: Enable Redis pipelining for batch operations

## Error Handling

The library provides comprehensive error handling:

- **Connection Failures**: Automatic reconnection with exponential backoff
- **Redis Outages**: Local cache fallback prevents service disruption
- **Invalid Messages**: Input validation with clear error reporting
- **Resource Exhaustion**: