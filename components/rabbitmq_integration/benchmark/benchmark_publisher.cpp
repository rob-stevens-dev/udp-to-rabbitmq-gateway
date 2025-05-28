// components/rabbitmq-integration/benchmark/benchmark_publisher.cpp
#include <benchmark/benchmark.h>
#include "rabbitmq_integration/publisher.hpp"
#include "rabbitmq_integration/message.hpp"
#include "rabbitmq_integration/local_queue.hpp"
#include <random>
#include <thread>

using namespace rabbitmq_integration;

// Global test configuration
static ConnectionConfig getTestConfig() {
    ConnectionConfig config;
    config.host = "localhost";
    config.port = 5672;
    config.username = "guest";
    config.password = "guest";
    config.vhost = "/benchmark";
    return config;
}

// Message creation benchmarks
static void BM_CreateJsonMessage(benchmark::State& state) {
    nlohmann::json testData = {
        {"device_id", "benchmark_device_123"},
        {"timestamp", 1234567890},
        {"latitude", 37.7749},
        {"longitude", -122.4194},
        {"speed", 35.5},
        {"battery", 0.85},
        {"sensors", {
            {"temperature", 22.5},
            {"humidity", 65.2},
            {"pressure", 1013.25}
        }}
    };
    
    for (auto _ : state) {
        Message msg(testData);
        benchmark::DoNotOptimize(msg);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * testData.dump().size());
}
BENCHMARK(BM_CreateJsonMessage);

static void BM_CreateTelemetryMessage(benchmark::State& state) {
    nlohmann::json telemetryData = {
        {"latitude", 40.7128},
        {"longitude", -74.0060},
        {"speed", 25.0},
        {"battery", 0.75}
    };
    
    for (auto _ : state) {
        auto msg = Message::createTelemetryMessage("us-east", "device123", telemetryData);
        benchmark::DoNotOptimize(msg);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CreateTelemetryMessage);

static void BM_MessageSerialization(benchmark::State& state) {
    nlohmann::json testData = {
        {"test", "benchmark"},
        {"value", 42},
        {"array", {1, 2, 3, 4, 5}}
    };
    Message msg(testData);
    
    for (auto _ : state) {
        auto serialized = msg.toDebugJson();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MessageSerialization);

// Local queue benchmarks
static void BM_LocalQueueEnqueue(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    config.overflowStrategy = LocalQueue::OverflowStrategy::DropOldest;
    LocalQueue queue(config);
    
    nlohmann::json testData = {{"benchmark", "enqueue"}};
    Message msg(testData);
    
    for (auto _ : state) {
        bool success = queue.enqueue("test.exchange", "test.routing.key", msg);
        benchmark::DoNotOptimize(success);
        
        // Clear queue periodically to avoid overflow
        if (state.iterations() % 1000 == 0) {
            queue.clear();
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LocalQueueEnqueue);

static void BM_LocalQueueDequeue(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    LocalQueue queue(config);
    
    // Pre-populate queue
    nlohmann::json testData = {{"benchmark", "dequeue"}};
    Message msg(testData);
    
    for (int i = 0; i < state.range(0); ++i) {
        queue.enqueue("test.exchange", "key" + std::to_string(i), msg);
    }
    
    for (auto _ : state) {
        auto result = queue.dequeue();
        benchmark::DoNotOptimize(result);
        
        // Re-populate if empty
        if (!result.has_value()) {
            for (int i = 0; i < 1000; ++i) {
                queue.enqueue("test.exchange", "key" + std::to_string(i), msg);
            }
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LocalQueueDequeue)->Range(1000, 10000);

static void BM_LocalQueuePriorityDequeue(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    LocalQueue queue(config);
    
    // Pre-populate with mixed priorities
    nlohmann::json testData = {{"benchmark", "priority"}};
    Message msg(testData);
    
    std::vector<MessagePriority> priorities = {
        MessagePriority::Low, MessagePriority::Normal, 
        MessagePriority::High, MessagePriority::Critical
    };
    
    for (int i = 0; i < state.range(0); ++i) {
        MessagePriority priority = priorities[i % priorities.size()];
        queue.enqueueWithPriority("test.exchange", "key" + std::to_string(i), msg, priority);
    }
    
    for (auto _ : state) {
        auto result = queue.dequeueHighestPriority();
        benchmark::DoNotOptimize(result);
        
        // Re-populate if empty
        if (!result.has_value()) {
            for (int i = 0; i < 1000; ++i) {
                MessagePriority priority = priorities[i % priorities.size()];
                queue.enqueueWithPriority("test.exchange", "key" + std::to_string(i), msg, priority);
            }
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LocalQueuePriorityDequeue)->Range(1000, 10000);

static void BM_LocalQueueBatchOperations(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    LocalQueue queue(config);
    
    nlohmann::json testData = {{"benchmark", "batch"}};
    Message msg(testData);
    
    size_t batchSize = state.range(0);
    
    for (auto _ : state) {
        // Batch enqueue
        std::vector<std::tuple<std::string, std::string, Message>> batch;
        batch.reserve(batchSize);
        
        for (size_t i = 0; i < batchSize; ++i) {
            batch.emplace_back("test.exchange", "key" + std::to_string(i), msg);
        }
        
        bool success = queue.enqueueBatch(batch);
        benchmark::DoNotOptimize(success);
        
        // Batch dequeue
        auto dequeued = queue.dequeueBatch(batchSize);
        benchmark::DoNotOptimize(dequeued);
    }
    
    state.SetItemsProcessed(state.iterations() * batchSize * 2); // enqueue + dequeue
}
BENCHMARK(BM_LocalQueueBatchOperations)->Range(10, 1000);

// Publisher benchmarks (using mock/local mode)
class BenchmarkPublisher {
public:
    BenchmarkPublisher() {
        PublisherConfig config;
        config.host = "localhost"; 
        config.port = 5672;
        config.username = "guest";
        config.password = "guest";
        config.confirmEnabled = false; // Disable for max performance
        config.localQueueSize = 100000;
        config.batchSize = 100;
        
        publisher_ = std::make_unique<Publisher>(config);
        
        // Don't actually connect for benchmarks - just use local queue
        // publisher_->start();
    }
    
    Publisher* get() { return publisher_.get(); }

private:
    std::unique_ptr<Publisher> publisher_;
};

static void BM_PublisherTelemetryMessage(benchmark::State& state) {
    static BenchmarkPublisher benchmarkPublisher;
    
    nlohmann::json telemetryData = {
        {"latitude", 37.7749},
        {"longitude", -122.4194},
        {"speed", 30.0},
        {"battery", 0.80}
    };
    
    std::vector<std::string> regions = {"us-west", "us-east", "eu-central", "ap-south"};
    std::vector<std::string> devices = {"device001", "device002", "device003", "device004"};
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> regionDist(0, regions.size() - 1);
    std::uniform_int_distribution<> deviceDist(0, devices.size() - 1);
    
    for (auto _ : state) {
        std::string region = regions[regionDist(gen)];
        std::string device = devices[deviceDist(gen)];
        
        // This will go to local queue since we're not connected
        bool success = benchmarkPublisher.get()->publishToInbound(region, device, telemetryData);
        benchmark::DoNotOptimize(success);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PublisherTelemetryMessage);

static void BM_PublisherBatchMessages(benchmark::State& state) {
    static BenchmarkPublisher benchmarkPublisher;
    
    nlohmann::json testData = {{"batch", "benchmark"}};
    size_t batchSize = state.range(0);
    
    for (auto _ : state) {
        auto batchPublisher = benchmarkPublisher.get()->createBatchPublisher();
        
        for (size_t i = 0; i < batchSize; ++i) {
            Message msg(testData);
            bool success = batchPublisher->addMessage("test.exchange", "test.key", msg);
            benchmark::DoNotOptimize(success);
        }
        
        bool published = batchPublisher->publish();
        benchmark::DoNotOptimize(published);
    }
    
    state.SetItemsProcessed(state.iterations() * batchSize);
}
BENCHMARK(BM_PublisherBatchMessages)->Range(10, 1000);

// Memory usage benchmarks
static void BM_MessageMemoryUsage(benchmark::State& state) {
    size_t messageSize = state.range(0);
    
    // Create large JSON message
    nlohmann::json largeData;
    for (size_t i = 0; i < messageSize; ++i) {
        largeData["field_" + std::to_string(i)] = "value_" + std::to_string(i);
    }
    
    for (auto _ : state) {
        Message msg(largeData);
        size_t totalSize = msg.getTotalSize();
        benchmark::DoNotOptimize(totalSize);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * largeData.dump().size());
}
BENCHMARK(BM_MessageMemoryUsage)->Range(100, 10000);

// Concurrent access benchmarks
static void BM_LocalQueueConcurrentAccess(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    static LocalQueue queue(config);
    
    nlohmann::json testData = {{"concurrent", "test"}};
    Message msg(testData);
    
    if (state.thread_index() == 0) {
        // Producer thread
        for (auto _ : state) {
            bool success = queue.enqueue("test.exchange", "concurrent.key", msg);
            benchmark::DoNotOptimize(success);
        }
    } else {
        // Consumer thread
        for (auto _ : state) {
            auto result = queue.dequeue();
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LocalQueueConcurrentAccess)->ThreadRange(2, 8);

// Throughput benchmarks
static void BM_MessageThroughput(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 1000000;
    LocalQueue queue(config);
    
    nlohmann::json testData = {
        {"throughput", "test"},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    
    size_t messagesPerIteration = state.range(0);
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Prepare messages
        std::vector<Message> messages;
        messages.reserve(messagesPerIteration);
        for (size_t i = 0; i < messagesPerIteration; ++i) {
            testData["id"] = i;
            messages.emplace_back(testData);
        }
        
        state.ResumeTiming();
        
        // Measure throughput
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& msg : messages) {
            queue.enqueue("throughput.exchange", "throughput.key", msg);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        state.SetIterationTime(duration.count() / 1000000.0); // Convert to seconds
    }
    
    state.SetItemsProcessed(state.iterations() * messagesPerIteration);
    state.SetBytesProcessed(state.items_processed() * testData.dump().size());
    
    // Calculate and report messages per second
    double messagesPerSecond = state.items_processed() / state.elapsed_cpu_time();
    state.counters["Messages/sec"] = benchmark::Counter(messagesPerSecond, benchmark::Counter::kIsRate);
}
BENCHMARK(BM_MessageThroughput)->Range(100, 10000)->UseManualTime();

// Latency benchmarks
static void BM_MessageLatency(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 10000;
    LocalQueue queue(config);
    
    nlohmann::json testData = {{"latency", "test"}};
    Message msg(testData);
    
    std::vector<double> latencies;
    latencies.reserve(state.max_iterations);
    
    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Enqueue and immediately dequeue to measure latency
        queue.enqueue("latency.exchange", "latency.key", msg);
        auto result = queue.dequeue();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        latencies.push_back(latency.count());
        benchmark::DoNotOptimize(result);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double p50 = latencies[latencies.size() * 0.5];
    double p95 = latencies[latencies.size() * 0.95];
    double p99 = latencies[latencies.size() * 0.99];
    
    state.counters["Mean(ns)"] = benchmark::Counter(mean);
    state.counters["P50(ns)"] = benchmark::Counter(p50);
    state.counters["P95(ns)"] = benchmark::Counter(p95);
    state.counters["P99(ns)"] = benchmark::Counter(p99);
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MessageLatency);

// Real-world scenario benchmarks
static void BM_TelemetryWorkload(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    LocalQueue queue(config);
    
    // Simulate realistic telemetry data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> latDist(35.0, 45.0);  // Latitude range
    std::uniform_real_distribution<> lngDist(-125.0, -115.0); // Longitude range
    std::uniform_real_distribution<> speedDist(0.0, 80.0);    // Speed in mph
    std::uniform_real_distribution<> batteryDist(0.1, 1.0);   // Battery level
    
    std::vector<std::string> devices;
    for (int i = 0; i < 100; ++i) {
        devices.push_back("device_" + std::to_string(i));
    }
    
    std::uniform_int_distribution<> deviceDist(0, devices.size() - 1);
    
    for (auto _ : state) {
        // Generate realistic telemetry message
        nlohmann::json telemetryData = {
            {"latitude", latDist(gen)},
            {"longitude", lngDist(gen)},
            {"speed", speedDist(gen)},
            {"battery", batteryDist(gen)},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"satellites", 8 + (gen() % 5)},
            {"hdop", 1.0 + (gen() % 30) * 0.1}
        };
        
        std::string deviceId = devices[deviceDist(gen)];
        auto msg = Message::createTelemetryMessage("us-west", deviceId, telemetryData);
        
        bool success = queue.enqueue("telemetry.exchange", 
                                    "us-west." + deviceId + "_inbound", 
                                    msg);
        benchmark::DoNotOptimize(success);
        
        // Occasionally dequeue to simulate processing
        if (state.iterations() % 10 == 0) {
            auto result = queue.dequeue();
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TelemetryWorkload);

static void BM_MixedMessageTypes(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 100000;
    LocalQueue queue(config);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> typeDist(0, 3);
    
    nlohmann::json telemetryData = {{"lat", 37.7749}, {"lng", -122.4194}};
    nlohmann::json commandData = {{"command", "SET_INTERVAL"}, {"value", 60}};
    std::string debugInfo = "Debug message for testing";
    std::vector<uint8_t> discardData = {0x42, 0x41, 0x44};
    
    for (auto _ : state) {
        Message msg(nlohmann::json{});
        std::string exchange, routingKey;
        
        switch (typeDist(gen)) {
            case 0: // Telemetry (70% of messages)
                if (gen() % 10 < 7) {
                    msg = Message::createTelemetryMessage("us-west", "device123", telemetryData);
                    exchange = "telemetry.exchange";
                    routingKey = "us-west.device123_inbound";
                }
                break;
                
            case 1: // Command (20% of messages)
                if (gen() % 10 < 2) {
                    msg = Message::createCommandMessage("us-west", "device123", "SET_INTERVAL", commandData);
                    exchange = "command.exchange";
                    routingKey = "us-west.device123_outbound";
                }
                break;
                
            case 2: // Debug (8% of messages)
                if (gen() % 100 < 8) {
                    msg = Message::createDebugMessage("us-west", "device123", debugInfo);
                    exchange = "telemetry.exchange";
                    routingKey = "us-west.device123_debug";
                }
                break;
                
            case 3: // Discard (2% of messages)
                if (gen() % 100 < 2) {
                    msg = Message::createDiscardMessage("us-west", "device123", "Parse error", discardData);
                    exchange = "telemetry.exchange";
                    routingKey = "us-west.device123_discard";
                }
                break;
        }
        
        if (!exchange.empty()) {
            bool success = queue.enqueue(exchange, routingKey, msg);
            benchmark::DoNotOptimize(success);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MixedMessageTypes);

// Memory pressure benchmarks
static void BM_MemoryPressure(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = state.range(0);
    config.maxMemoryBytes = state.range(0) * 1024; // 1KB per message limit
    config.overflowStrategy = LocalQueue::OverflowStrategy::DropOldest;
    LocalQueue queue(config);
    
    // Create large messages to put pressure on memory
    nlohmann::json largeData;
    for (int i = 0; i < 100; ++i) {
        largeData["field_" + std::to_string(i)] = std::string(100, 'x'); // 100 bytes per field
    }
    Message largeMsg(largeData);
    
    for (auto _ : state) {
        bool success = queue.enqueue("memory.exchange", "memory.key", largeMsg);
        benchmark::DoNotOptimize(success);
        
        // Check memory usage
        size_t memUsage = queue.memoryUsage();
        benchmark::DoNotOptimize(memUsage);
        
        // Occasionally dequeue to test memory cleanup
        if (state.iterations() % 10 == 0) {
            auto result = queue.dequeue();
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
    state.counters["FinalMemoryMB"] = benchmark::Counter(
        queue.memoryUsage() / (1024.0 * 1024.0));
    state.counters["FinalQueueSize"] = benchmark::Counter(queue.size());
}
BENCHMARK(BM_MemoryPressure)->Range(1000, 10000);

// Stress test benchmarks
static void BM_StressTest(benchmark::State& state) {
    LocalQueue::Config config;
    config.maxSize = 1000000;
    static LocalQueue queue(config);
    
    nlohmann::json testData = {{"stress", "test"}};
    Message msg(testData);
    
    // High-frequency mixed operations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> opDist(0, 2);
    
    for (auto _ : state) {
        switch (opDist(gen)) {
            case 0: // Enqueue
                queue.enqueue("stress.exchange", "stress.key", msg);
                break;
            case 1: // Dequeue
                queue.dequeue();
                break;
            case 2: // Check size
                queue.size();
                break;
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StressTest)->ThreadRange(1, 16);

// Publisher pool benchmarks (when available)
#ifdef BENCHMARK_WITH_RABBITMQ
static void BM_PublisherPoolThroughput(benchmark::State& state) {
    auto config = getTestConfig();
    PublisherConfig pubConfig(config);
    pubConfig.confirmEnabled = false; // Max throughput
    pubConfig.batchSize = 100;
    
    PublisherPool pool(pubConfig, state.range(0)); // Number of publishers
    
    if (!pool.start()) {
        state.SkipWithError("Failed to start publisher pool");
        return;
    }
    
    nlohmann::json testData = {{"pool", "benchmark"}};
    
    for (auto _ : state) {
        bool success = pool.publishToInbound("benchmark", "device123", testData);
        benchmark::DoNotOptimize(success);
    }
    
    auto stats = pool.getAggregatedStats();
    state.counters["MessagesPublished"] = benchmark::Counter(stats.messagesPublished);
    state.counters["PublishFailed"] = benchmark::Counter(stats.publishFailed);
    
    pool.stop();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PublisherPoolThroughput)->Range(1, 8);
#endif

// Custom main function for benchmark configuration
int main(int argc, char** argv) {
    // Configure benchmark settings
    benchmark::Initialize(&argc, argv);
    
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    
    // Set up logging for benchmarks
    spdlog::set_level(spdlog::level::warn); // Reduce log noise during benchmarks
    
    // Add custom benchmark configuration
    benchmark::AddCustomContext("RabbitMQ Integration", "v0.1.0");
    benchmark::AddCustomContext("Build Type", CMAKE_BUILD_TYPE);
    benchmark::AddCustomContext("Compiler", CMAKE_CXX_COMPILER_ID);
    
    // Run benchmarks
    benchmark::RunSpecifiedBenchmarks();
    
    // Generate report
    benchmark::Shutdown();
    return 0;
}