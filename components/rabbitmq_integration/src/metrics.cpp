// components/rabbitmq-integration/src/metrics.cpp
#include "rabbitmq_integration/metrics.hpp"
#include <spdlog/spdlog.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>
#include <prometheus/gateway.h>
#include <chrono>

namespace rabbitmq_integration {
namespace metrics {

// Prometheus metric definitions
class PrometheusMetrics {
public:
    PrometheusMetrics(std::shared_ptr<prometheus::Registry> registry) 
        : registry_(registry) {
        initializeMetrics();
    }
    
    // Connection metrics
    prometheus::Counter& getConnectionAttempts() { return *connectionAttempts_; }
    prometheus::Counter& getConnectionSuccesses() { return *connectionSuccesses_; }
    prometheus::Counter& getConnectionFailures() { return *connectionFailures_; }
    prometheus::Gauge& getCurrentConnections() { return *currentConnections_; }
    prometheus::Histogram& getConnectionLatency() { return *connectionLatency_; }
    
    // Publisher metrics
    prometheus::Counter& getMessagesPublished() { return *messagesPublished_; }
    prometheus::Counter& getPublishFailures() { return *publishFailures_; }
    prometheus::Counter& getPublishConfirms() { return *publishConfirms_; }
    prometheus::Counter& getPublishRetries() { return *publishRetries_; }
    prometheus::Histogram& getPublishLatency() { return *publishLatency_; }
    prometheus::Gauge& getLocalQueueSize() { return *localQueueSize_; }
    prometheus::Counter& getBatchesPublished() { return *batchesPublished_; }
    
    // Consumer metrics
    prometheus::Counter& getMessagesConsumed() { return *messagesConsumed_; }
    prometheus::Counter& getMessagesAcknowledged() { return *messagesAcknowledged_; }
    prometheus::Counter& getMessagesRejected() { return *messagesRejected_; }
    prometheus::Counter& getConsumerCancellations() { return *consumerCancellations_; }
    prometheus::Histogram& getProcessingLatency() { return *processingLatency_; }
    prometheus::Gauge& getActiveConsumers() { return *activeConsumers_; }
    
    // Queue metrics
    prometheus::Counter& getQueuesCreated() { return *queuesCreated_; }
    prometheus::Counter& getQueuesDeleted() { return *queuesDeleted_; }
    prometheus::Counter& getQueuesPurged() { return *queuesPurged_; }
    prometheus::Gauge& getQueueDepth() { return *queueDepth_; }
    
    // System metrics
    prometheus::Gauge& getMemoryUsage() { return *memoryUsage_; }
    prometheus::Gauge& getCpuUsage() { return *cpuUsage_; }
    prometheus::Counter& getErrors() { return *errors_; }

private:
    std::shared_ptr<prometheus::Registry> registry_;
    
    // Connection metrics
    prometheus::Counter* connectionAttempts_;
    prometheus::Counter* connectionSuccesses_;
    prometheus::Counter* connectionFailures_;
    prometheus::Gauge* currentConnections_;
    prometheus::Histogram* connectionLatency_;
    
    // Publisher metrics
    prometheus::Counter* messagesPublished_;
    prometheus::Counter* publishFailures_;
    prometheus::Counter* publishConfirms_;
    prometheus::Counter* publishRetries_;
    prometheus::Histogram* publishLatency_;
    prometheus::Gauge* localQueueSize_;
    prometheus::Counter* batchesPublished_;
    
    // Consumer metrics
    prometheus::Counter* messagesConsumed_;
    prometheus::Counter* messagesAcknowledged_;
    prometheus::Counter* messagesRejected_;
    prometheus::Counter* consumerCancellations_;
    prometheus::Histogram* processingLatency_;
    prometheus::Gauge* activeConsumers_;
    
    // Queue metrics
    prometheus::Counter* queuesCreated_;
    prometheus::Counter* queuesDeleted_;
    prometheus::Counter* queuesPurged_;
    prometheus::Gauge* queueDepth_;
    
    // System metrics
    prometheus::Gauge* memoryUsage_;
    prometheus::Gauge* cpuUsage_;
    prometheus::Counter* errors_;
    
    void initializeMetrics() {
        // Connection metrics
        auto& connectionFamily = prometheus::BuildCounter()
            .Name("rabbitmq_integration_connections_total")
            .Help("Total number of connection attempts")
            .Register(*registry_);
        
        connectionAttempts_ = &connectionFamily.Add({{"type", "attempts"}});
        connectionSuccesses_ = &connectionFamily.Add({{"type", "successes"}});
        connectionFailures_ = &connectionFamily.Add({{"type", "failures"}});
        
        currentConnections_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_current_connections")
            .Help("Current number of active connections")
            .Register(*registry_)
            .Add({});
        
        connectionLatency_ = &prometheus::BuildHistogram()
            .Name("rabbitmq_integration_connection_duration_seconds")
            .Help("Time taken to establish connections")
            .Buckets({0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0})
            .Register(*registry_)
            .Add({});
        
        // Publisher metrics
        auto& publishFamily = prometheus::BuildCounter()
            .Name("rabbitmq_integration_messages_published_total")
            .Help("Total number of messages published")
            .Register(*registry_);
        
        messagesPublished_ = &publishFamily.Add({{"status", "success"}});
        publishFailures_ = &publishFamily.Add({{"status", "failure"}});
        publishConfirms_ = &publishFamily.Add({{"status", "confirmed"}});
        publishRetries_ = &publishFamily.Add({{"status", "retried"}});
        
        publishLatency_ = &prometheus::BuildHistogram()
            .Name("rabbitmq_integration_publish_duration_seconds")
            .Help("Time taken to publish messages")
            .Buckets({0.0001, 0.0005, 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5})
            .Register(*registry_)
            .Add({});
        
        localQueueSize_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_local_queue_size")
            .Help("Current size of local message queue")
            .Register(*registry_)
            .Add({});
        
        batchesPublished_ = &prometheus::BuildCounter()
            .Name("rabbitmq_integration_batches_published_total")
            .Help("Total number of message batches published")
            .Register(*registry_)
            .Add({});
        
        // Consumer metrics
        auto& consumeFamily = prometheus::BuildCounter()
            .Name("rabbitmq_integration_messages_consumed_total")
            .Help("Total number of messages consumed")
            .Register(*registry_);
        
        messagesConsumed_ = &consumeFamily.Add({{"status", "received"}});
        messagesAcknowledged_ = &consumeFamily.Add({{"status", "acknowledged"}});
        messagesRejected_ = &consumeFamily.Add({{"status", "rejected"}});
        
        consumerCancellations_ = &prometheus::BuildCounter()
            .Name("rabbitmq_integration_consumer_cancellations_total")
            .Help("Total number of consumer cancellations")
            .Register(*registry_)
            .Add({});
        
        processingLatency_ = &prometheus::BuildHistogram()
            .Name("rabbitmq_integration_message_processing_duration_seconds")
            .Help("Time taken to process messages")
            .Buckets({0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0})
            .Register(*registry_)
            .Add({});
        
        activeConsumers_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_active_consumers")
            .Help("Current number of active consumers")
            .Register(*registry_)
            .Add({});
        
        // Queue metrics
        auto& queueFamily = prometheus::BuildCounter()
            .Name("rabbitmq_integration_queue_operations_total")
            .Help("Total number of queue operations")
            .Register(*registry_);
        
        queuesCreated_ = &queueFamily.Add({{"operation", "created"}});
        queuesDeleted_ = &queueFamily.Add({{"operation", "deleted"}});
        queuesPurged_ = &queueFamily.Add({{"operation", "purged"}});
        
        queueDepth_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_queue_depth")
            .Help("Current depth of message queues")
            .Register(*registry_)
            .Add({});
        
        // System metrics
        memoryUsage_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_memory_usage_bytes")
            .Help("Current memory usage")
            .Register(*registry_)
            .Add({});
        
        cpuUsage_ = &prometheus::BuildGauge()
            .Name("rabbitmq_integration_cpu_usage_percent")
            .Help("Current CPU usage percentage")
            .Register(*registry_)
            .Add({});
        
        errors_ = &prometheus::BuildCounter()
            .Name("rabbitmq_integration_errors_total")
            .Help("Total number of errors")
            .Register(*registry_)
            .Add({});
    }
};

// MetricsCollector implementation
class MetricsCollector::Impl {
public:
    Impl(const MetricsConfig& config) 
        : config_(config), running_(false) {
        
        if (config_.enablePrometheus) {
            registry_ = std::make_shared<prometheus::Registry>();
            prometheusMetrics_ = std::make_unique<PrometheusMetrics>(registry_);
            
            if (config_.enableGateway && !config_.gatewayUrl.empty()) {
                gateway_ = std::make_unique<prometheus::Gateway>(
                    config_.gatewayUrl, config_.jobName, config_.labels);
            }
        }
    }
    
    ~Impl() {
        stop();
    }
    
    bool start() {
        if (running_) return true;
        
        running_ = true;
        
        if (config_.enablePrometheus && config_.metricsPort > 0) {
            // Start HTTP server for metrics endpoint
            try {
                metricsThread_ = std::thread([this]() {
                    runMetricsServer();
                });
            } catch (const std::exception& e) {
                spdlog::error("Failed to start metrics server: {}", e.what());
                return false;
            }
        }
        
        // Start collection thread
        collectionThread_ = std::thread([this]() {
            collectMetrics();
        });
        
        spdlog::info("Metrics collector started");
        return true;
    }
    
    void stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (metricsThread_.joinable()) {
            metricsThread_.join();
        }
        
        if (collectionThread_.joinable()) {
            collectionThread_.join();
        }
        
        spdlog::info("Metrics collector stopped");
    }
    
    void recordConnectionAttempt() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getConnectionAttempts().Increment();
        }
        connectionMetrics_.attempts++;
    }
    
    void recordConnectionSuccess(std::chrono::milliseconds latency) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getConnectionSuccesses().Increment();
            prometheusMetrics_->getConnectionLatency().Observe(latency.count() / 1000.0);
            prometheusMetrics_->getCurrentConnections().Increment();
        }
        connectionMetrics_.successes++;
        connectionMetrics_.totalLatency += latency;
    }
    
    void recordConnectionFailure() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getConnectionFailures().Increment();
        }
        connectionMetrics_.failures++;
    }
    
    void recordConnectionClosed() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getCurrentConnections().Decrement();
        }
    }
    
    void recordMessagePublished(std::chrono::milliseconds latency) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getMessagesPublished().Increment();
            prometheusMetrics_->getPublishLatency().Observe(latency.count() / 1000.0);
        }
        publishMetrics_.messagesPublished++;
        publishMetrics_.totalLatency += latency;
    }
    
    void recordPublishFailure() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getPublishFailures().Increment();
        }
        publishMetrics_.failures++;
    }
    
    void recordPublishConfirm() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getPublishConfirms().Increment();
        }
        publishMetrics_.confirms++;
    }
    
    void recordPublishRetry() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getPublishRetries().Increment();
        }
        publishMetrics_.retries++;
    }
    
    void recordBatchPublished(size_t batchSize) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getBatchesPublished().Increment();
        }
        publishMetrics_.batchesPublished++;
        publishMetrics_.totalBatchSize += batchSize;
    }
    
    void updateLocalQueueSize(size_t size) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getLocalQueueSize().Set(size);
        }
        publishMetrics_.localQueueSize = size;
    }
    
    void recordMessageConsumed(std::chrono::milliseconds processingTime) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getMessagesConsumed().Increment();
            prometheusMetrics_->getProcessingLatency().Observe(processingTime.count() / 1000.0);
        }
        consumerMetrics_.messagesConsumed++;
        consumerMetrics_.totalProcessingTime += processingTime;
    }
    
    void recordMessageAcknowledged() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getMessagesAcknowledged().Increment();
        }
        consumerMetrics_.messagesAcknowledged++;
    }
    
    void recordMessageRejected() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getMessagesRejected().Increment();
        }
        consumerMetrics_.messagesRejected++;
    }
    
    void recordConsumerCancellation() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getConsumerCancellations().Increment();
        }
        consumerMetrics_.cancellations++;
    }
    
    void updateActiveConsumers(size_t count) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getActiveConsumers().Set(count);
        }
        consumerMetrics_.activeConsumers = count;
    }
    
    void recordQueueCreated() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getQueuesCreated().Increment();
        }
        queueMetrics_.queuesCreated++;
    }
    
    void recordQueueDeleted() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getQueuesDeleted().Increment();
        }
        queueMetrics_.queuesDeleted++;
    }
    
    void recordQueuePurged() {
        if (prometheusMetrics_) {
            prometheusMetrics_->getQueuesPurged().Increment();
        }
        queueMetrics_.queuesPurged++;
    }
    
    void updateQueueDepth(const std::string& queueName, size_t depth) {
        if (prometheusMetrics_) {
            // For Prometheus, we might want per-queue metrics
            // For now, we'll use the total across all queues
            size_t totalDepth = 0;
            {
                std::lock_guard<std::mutex> lock(queueDepthMutex_);
                queueDepths_[queueName] = depth;
                for (const auto& pair : queueDepths_) {
                    totalDepth += pair.second;
                }
            }
            prometheusMetrics_->getQueueDepth().Set(totalDepth);
        }
        queueMetrics_.totalDepth = depth; // Simplified for now
    }
    
    void recordError(const std::string& context, const std::string& error) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getErrors().Increment();
        }
        systemMetrics_.errors++;
        
        spdlog::error("Metrics: Error in {}: {}", context, error);
    }
    
    void updateMemoryUsage(size_t bytes) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getMemoryUsage().Set(bytes);
        }
        systemMetrics_.memoryUsage = bytes;
    }
    
    void updateCpuUsage(double percentage) {
        if (prometheusMetrics_) {
            prometheusMetrics_->getCpuUsage().Set(percentage);
        }
        systemMetrics_.cpuUsage = percentage;
    }
    
    MetricsSnapshot getSnapshot() const {
        MetricsSnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::now();
        snapshot.connection = connectionMetrics_;
        snapshot.publisher = publishMetrics_;
        snapshot.consumer = consumerMetrics_;
        snapshot.queue = queueMetrics_;
        snapshot.system = systemMetrics_;
        return snapshot;
    }
    
    bool pushToGateway() {
        if (!gateway_ || !registry_) {
            return false;
        }
        
        try {
            auto result = gateway_->Push();
            if (result != prometheus::Gateway::HttpCode::Ok) {
                spdlog::error("Failed to push metrics to gateway: {}", static_cast<int>(result));
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Exception pushing metrics to gateway: {}", e.what());
            return false;
        }
    }

private:
    MetricsConfig config_;
    std::atomic<bool> running_;
    
    // Prometheus components
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<PrometheusMetrics> prometheusMetrics_;
    std::unique_ptr<prometheus::Gateway> gateway_;
    
    // Threads
    std::thread metricsThread_;
    std::thread collectionThread_;
    
    // Internal metrics storage
    ConnectionMetrics connectionMetrics_;
    PublisherMetrics publishMetrics_;
    ConsumerMetrics consumerMetrics_;
    QueueMetrics queueMetrics_;
    SystemMetrics systemMetrics_;
    
    // Queue depth tracking
    std::mutex queueDepthMutex_;
    std::map<std::string, size_t> queueDepths_;
    
    void runMetricsServer() {
        // This would typically use a web server library like cpp-httplib
        // For now, we'll simulate the server
        spdlog::info("Metrics server running on port {}", config_.metricsPort);
        
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // In a real implementation, this would serve HTTP requests
            // For demonstration, we'll just log periodically
            if (config_.enablePeriodicLogging) {
                logMetrics();
            }
        }
    }
    
    void collectMetrics() {
        while (running_) {
            try {
                // Collect system metrics
                collectSystemMetrics();
                
                // Push to gateway if configured
                if (config_.enableGateway && config_.pushInterval > std::chrono::seconds(0)) {
                    static auto lastPush = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    
                    if (now - lastPush >= config_.pushInterval) {
                        pushToGateway();
                        lastPush = now;
                    }
                }
                
            } catch (const std::exception& e) {
                spdlog::error("Exception in metrics collection: {}", e.what());
            }
            
            std::this_thread::sleep_for(config_.collectionInterval);
        }
    }
    
    void collectSystemMetrics() {
        // Collect memory usage
        updateMemoryUsage(getCurrentMemoryUsage());
        
        // Collect CPU usage
        updateCpuUsage(getCurrentCpuUsage());
    }
    
    size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage collection
        // This is a simplified implementation
#ifdef __linux__
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoull(value) * 1024; // Convert KB to bytes
            }
        }
#endif
        return 0;
    }
    
    double getCurrentCpuUsage() {
        // Platform-specific CPU usage collection
        // This is a simplified implementation
        static auto lastTime = std::chrono::steady_clock::now();
        static auto lastCpuTime = std::clock();
        
        auto now = std::chrono::steady_clock::now();
        auto cpuTime = std::clock();
        
        auto wallTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime);
        auto usedCpuTime = (cpuTime - lastCpuTime) * 1000 / CLOCKS_PER_SEC;
        
        double usage = 0.0;
        if (wallTime.count() > 0) {
            usage = (double)usedCpuTime / wallTime.count() * 100.0;
        }
        
        lastTime = now;
        lastCpuTime = cpuTime;
        
        return std::min(100.0, std::max(0.0, usage));
    }
    
    void logMetrics() {
        auto snapshot = getSnapshot();
        
        spdlog::info("=== RabbitMQ Integration Metrics ===");
        spdlog::info("Connections: {} attempts, {} successes, {} failures", 
                    snapshot.connection.attempts, 
                    snapshot.connection.successes, 
                    snapshot.connection.failures);
        
        spdlog::info("Publisher: {} published, {} failed, {} confirmed, {} retries", 
                    snapshot.publisher.messagesPublished,
                    snapshot.publisher.failures,
                    snapshot.publisher.confirms,
                    snapshot.publisher.retries);
        
        spdlog::info("Consumer: {} consumed, {} acked, {} rejected", 
                    snapshot.consumer.messagesConsumed,
                    snapshot.consumer.messagesAcknowledged,
                    snapshot.consumer.messagesRejected);
        
        spdlog::info("Queues: {} created, {} deleted, depth: {}", 
                    snapshot.queue.queuesCreated,
                    snapshot.queue.queuesDeleted,
                    snapshot.queue.totalDepth);
        
        spdlog::info("System: Memory: {} MB, CPU: {:.1f}%, Errors: {}", 
                    snapshot.system.memoryUsage / (1024 * 1024),
                    snapshot.system.cpuUsage,
                    snapshot.system.errors);
        
        spdlog::info("====================================");
    }
};

// MetricsCollector public interface
MetricsCollector::MetricsCollector(const MetricsConfig& config) 
    : impl_(std::make_unique<Impl>(config)) {
}

MetricsCollector::~MetricsCollector() = default;

bool MetricsCollector::start() {
    return impl_->start();
}

void MetricsCollector::stop() {
    impl_->stop();
}

void MetricsCollector::recordConnectionAttempt() {
    impl_->recordConnectionAttempt();
}

void MetricsCollector::recordConnectionSuccess(std::chrono::milliseconds latency) {
    impl_->recordConnectionSuccess(latency);
}

void MetricsCollector::recordConnectionFailure() {
    impl_->recordConnectionFailure();
}

void MetricsCollector::recordConnectionClosed() {
    impl_->recordConnectionClosed();
}

void MetricsCollector::recordMessagePublished(std::chrono::milliseconds latency) {
    impl_->recordMessagePublished(latency);
}

void MetricsCollector::recordPublishFailure() {
    impl_->recordPublishFailure();
}

void MetricsCollector::recordPublishConfirm() {
    impl_->recordPublishConfirm();
}

void MetricsCollector::recordPublishRetry() {
    impl_->recordPublishRetry();
}

void MetricsCollector::recordBatchPublished(size_t batchSize) {
    impl_->recordBatchPublished(batchSize);
}

void MetricsCollector::updateLocalQueueSize(size_t size) {
    impl_->updateLocalQueueSize(size);
}

void MetricsCollector::recordMessageConsumed(std::chrono::milliseconds processingTime) {
    impl_->recordMessageConsumed(processingTime);
}

void MetricsCollector::recordMessageAcknowledged() {
    impl_->recordMessageAcknowledged();
}

void MetricsCollector::recordMessageRejected() {
    impl_->recordMessageRejected();
}

void MetricsCollector::recordConsumerCancellation() {
    impl_->recordConsumerCancellation();
}

void MetricsCollector::updateActiveConsumers(size_t count) {
    impl_->updateActiveConsumers(count);
}

void MetricsCollector::recordQueueCreated() {
    impl_->recordQueueCreated();
}

void MetricsCollector::recordQueueDeleted() {
    impl_->recordQueueDeleted();
}

void MetricsCollector::recordQueuePurged() {
    impl_->recordQueuePurged();
}

void MetricsCollector::updateQueueDepth(const std::string& queueName, size_t depth) {
    impl_->updateQueueDepth(queueName, depth);
}

void MetricsCollector::recordError(const std::string& context, const std::string& error) {
    impl_->recordError(context, error);
}

void MetricsCollector::updateMemoryUsage(size_t bytes) {
    impl_->updateMemoryUsage(bytes);
}

void MetricsCollector::updateCpuUsage(double percentage) {
    impl_->updateCpuUsage(percentage);
}

MetricsSnapshot MetricsCollector::getSnapshot() const {
    return impl_->getSnapshot();
}

bool MetricsCollector::pushToGateway() {
    return impl_->pushToGateway();
}

// Global metrics instance
static std::unique_ptr<MetricsCollector> g_metricsCollector;
static std::mutex g_metricsCollectorMutex;

MetricsCollector& getGlobalMetricsCollector() {
    std::lock_guard<std::mutex> lock(g_metricsCollectorMutex);
    if (!g_metricsCollector) {
        MetricsConfig config;
        g_metricsCollector = std::make_unique<MetricsCollector>(config);
    }
    return *g_metricsCollector;
}

void initializeGlobalMetricsCollector(const MetricsConfig& config) {
    std::lock_guard<std::mutex> lock(g_metricsCollectorMutex);
    g_metricsCollector = std::make_unique<MetricsCollector>(config);
}

void shutdownGlobalMetricsCollector() {
    std::lock_guard<std::mutex> lock(g_metricsCollectorMutex);
    if (g_metricsCollector) {
        g_metricsCollector->stop();
        g_metricsCollector.reset();
    }
}

} // namespace metrics
} // namespace rabbitmq_integration