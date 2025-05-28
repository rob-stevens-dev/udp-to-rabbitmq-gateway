// components/rabbitmq-integration/include/rabbitmq_integration/metrics.hpp
#pragma once

#include <chrono>
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>
#include <sstream>
#include <iomanip>

namespace rabbitmq_integration {
namespace metrics {

// Metrics configuration
struct MetricsConfig {
    bool enablePrometheus = true;
    bool enableGateway = false;
    std::string gatewayUrl;
    std::string jobName = "rabbitmq-integration";
    std::map<std::string, std::string> labels;
    
    int metricsPort = 8080;
    std::string metricsPath = "/metrics";
    std::string healthPath = "/health";
    
    std::chrono::seconds collectionInterval = std::chrono::seconds(10);
    std::chrono::seconds pushInterval = std::chrono::seconds(60);
    
    bool enablePeriodicLogging = false;
    std::chrono::seconds loggingInterval = std::chrono::seconds(300);
};

// Individual metric structures
struct ConnectionMetrics {
    uint64_t attempts = 0;
    uint64_t successes = 0;
    uint64_t failures = 0;
    uint64_t reconnects = 0;
    std::chrono::milliseconds totalLatency{0};
    std::chrono::system_clock::time_point lastConnectionTime;
    std::chrono::system_clock::time_point lastDisconnectionTime;
};

struct PublisherMetrics {
    uint64_t messagesPublished = 0;
    uint64_t failures = 0;
    uint64_t confirms = 0;
    uint64_t retries = 0;
    uint64_t batchesPublished = 0;
    size_t totalBatchSize = 0;
    size_t localQueueSize = 0;
    std::chrono::milliseconds totalLatency{0};
    std::chrono::system_clock::time_point lastPublishTime;
};

struct ConsumerMetrics {
    uint64_t messagesConsumed = 0;
    uint64_t messagesAcknowledged = 0;
    uint64_t messagesRejected = 0;
    uint64_t messagesRequeued = 0;
    uint64_t cancellations = 0;
    size_t activeConsumers = 0;
    std::chrono::milliseconds totalProcessingTime{0};
    std::chrono::system_clock::time_point lastMessageTime;
};

struct QueueMetrics {
    uint64_t queuesCreated = 0;
    uint64_t queuesDeleted = 0;
    uint64_t queuesPurged = 0;
    size_t totalDepth = 0;
    std::map<std::string, size_t> queueDepths;
};

struct SystemMetrics {
    size_t memoryUsage = 0;
    double cpuUsage = 0.0;
    uint64_t errors = 0;
    std::chrono::system_clock::time_point startTime;
    std::chrono::milliseconds uptime{0};
};

// Combined metrics snapshot
struct MetricsSnapshot {
    std::chrono::system_clock::time_point timestamp;
    ConnectionMetrics connection;
    PublisherMetrics publisher;
    ConsumerMetrics consumer;
    QueueMetrics queue;
    SystemMetrics system;
};

/**
 * @brief Comprehensive metrics collection system for RabbitMQ integration
 */
class MetricsCollector {
public:
    explicit MetricsCollector(const MetricsConfig& config);
    ~MetricsCollector();
    
    // Non-copyable
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    
    // Lifecycle
    bool start();
    void stop();
    
    // Connection metrics
    void recordConnectionAttempt();
    void recordConnectionSuccess(std::chrono::milliseconds latency);
    void recordConnectionFailure();
    void recordConnectionClosed();
    
    // Publisher metrics
    void recordMessagePublished(std::chrono::milliseconds latency = std::chrono::milliseconds(0));
    void recordPublishFailure();
    void recordPublishConfirm();
    void recordPublishRetry();
    void recordBatchPublished(size_t batchSize);
    void updateLocalQueueSize(size_t size);
    
    // Consumer metrics
    void recordMessageConsumed(std::chrono::milliseconds processingTime = std::chrono::milliseconds(0));
    void recordMessageAcknowledged();
    void recordMessageRejected();
    void recordConsumerCancellation();
    void updateActiveConsumers(size_t count);
    
    // Queue metrics
    void recordQueueCreated();
    void recordQueueDeleted();
    void recordQueuePurged();
    void updateQueueDepth(const std::string& queueName, size_t depth);
    
    // System metrics
    void recordError(const std::string& context, const std::string& error);
    void updateMemoryUsage(size_t bytes);
    void updateCpuUsage(double percentage);
    
    // Data access
    MetricsSnapshot getSnapshot() const;
    
    // Prometheus integration
    bool pushToGateway();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief RAII timer for measuring operation latency
 */
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    std::chrono::milliseconds elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    }
    
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

/**
 * @brief Scoped timer that automatically records metrics
 */
template<typename Func>
class ScopedTimer {
public:
    ScopedTimer(Func&& func) : func_(std::forward<Func>(func)) {}
    
    ~ScopedTimer() {
        func_(timer_.elapsed());
    }

private:
    Timer timer_;
    Func func_;
};

// Helper macros for convenient metric recording
#define METRICS_RECORD_LATENCY(collector, operation) \
    auto scopedTimer = makeScopedTimer([&](auto latency) { \
        collector.record##operation(latency); \
    })

#define METRICS_TIME_OPERATION(collector, operation, code) \
    do { \
        Timer timer; \
        { code } \
        collector.record##operation(timer.elapsed()); \
    } while(0)

// Factory function for scoped timers
template<typename Func>
ScopedTimer<Func> makeScopedTimer(Func&& func) {
    return ScopedTimer<Func>(std::forward<Func>(func));
}

// Global metrics collector functions
MetricsCollector& getGlobalMetricsCollector();
void initializeGlobalMetricsCollector(const MetricsConfig& config);
void shutdownGlobalMetricsCollector();

/**
 * @brief Metrics integration helper for Publisher
 */
class PublisherMetricsHelper {
public:
    explicit PublisherMetricsHelper(MetricsCollector& collector) 
        : collector_(collector) {}
    
    void onPublishStart() {
        publishTimer_.reset();
    }
    
    void onPublishSuccess() {
        collector_.recordMessagePublished(publishTimer_.elapsed());
    }
    
    void onPublishFailure() {
        collector_.recordPublishFailure();
    }
    
    void onPublishConfirm() {
        collector_.recordPublishConfirm();
    }
    
    void onPublishRetry() {
        collector_.recordPublishRetry();
    }
    
    void onBatchPublished(size_t batchSize) {
        collector_.recordBatchPublished(batchSize);
    }
    
    void updateLocalQueueSize(size_t size) {
        collector_.updateLocalQueueSize(size);
    }

private:
    MetricsCollector& collector_;
    Timer publishTimer_;
};

/**
 * @brief Metrics integration helper for Consumer
 */
class ConsumerMetricsHelper {
public:
    explicit ConsumerMetricsHelper(MetricsCollector& collector) 
        : collector_(collector) {}
    
    void onMessageReceived() {
        processingTimer_.reset();
    }
    
    void onMessageProcessed() {
        collector_.recordMessageConsumed(processingTimer_.elapsed());
    }
    
    void onMessageAcknowledged() {
        collector_.recordMessageAcknowledged();
    }
    
    void onMessageRejected() {
        collector_.recordMessageRejected();
    }
    
    void onConsumerCancelled() {
        collector_.recordConsumerCancellation();
    }
    
    void updateActiveConsumers(size_t count) {
        collector_.updateActiveConsumers(count);
    }

private:
    MetricsCollector& collector_;
    Timer processingTimer_;
};

/**
 * @brief Metrics integration helper for Connection
 */
class ConnectionMetricsHelper {
public:
    explicit ConnectionMetricsHelper(MetricsCollector& collector) 
        : collector_(collector) {}
    
    void onConnectionAttempt() {
        connectionTimer_.reset();
        collector_.recordConnectionAttempt();
    }
    
    void onConnectionSuccess() {
        collector_.recordConnectionSuccess(connectionTimer_.elapsed());
    }
    
    void onConnectionFailure() {
        collector_.recordConnectionFailure();
    }
    
    void onConnectionClosed() {
        collector_.recordConnectionClosed();
    }

private:
    MetricsCollector& collector_;
    Timer connectionTimer_;
};

/**
 * @brief Health check utility for metrics system
 */
struct HealthStatus {
    bool healthy = true;
    std::string status = "OK";
    std::map<std::string, std::string> details;
    std::chrono::system_clock::time_point timestamp;
};

class HealthChecker {
public:
    explicit HealthChecker(const MetricsCollector& collector) 
        : collector_(collector) {}
    
    HealthStatus checkHealth() const {
        HealthStatus status;
        status.timestamp = std::chrono::system_clock::now();
        
        auto snapshot = collector_.getSnapshot();
        
        // Check connection health
        if (snapshot.connection.failures > snapshot.connection.successes * 0.1) {
            status.healthy = false;
            status.details["connection"] = "High failure rate";
        }
        
        // Check publish health
        if (snapshot.publisher.failures > snapshot.publisher.messagesPublished * 0.05) {
            status.healthy = false;
            status.details["publisher"] = "High publish failure rate";
        }
        
        // Check consumer health
        if (snapshot.consumer.messagesRejected > snapshot.consumer.messagesConsumed * 0.05) {
            status.healthy = false;
            status.details["consumer"] = "High message rejection rate";
        }
        
        // Check system resources
        if (snapshot.system.memoryUsage > 1024 * 1024 * 1024) { // 1GB
            status.details["memory"] = "High memory usage";
        }
        
        if (snapshot.system.cpuUsage > 80.0) {
            status.details["cpu"] = "High CPU usage";
        }
        
        if (snapshot.system.errors > 100) {
            status.healthy = false;
            status.details["errors"] = "High error count";
        }
        
        if (!status.healthy) {
            status.status = "UNHEALTHY";
        } else if (!status.details.empty()) {
            status.status = "WARNING";
        }
        
        return status;
    }

private:
    const MetricsCollector& collector_;
};

/**
 * @brief Metrics aggregator for multiple instances
 */
class MetricsAggregator {
public:
    void addSnapshot(const std::string& instanceId, const MetricsSnapshot& snapshot) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_[instanceId] = snapshot;
    }
    
    MetricsSnapshot getAggregatedSnapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        MetricsSnapshot aggregated;
        aggregated.timestamp = std::chrono::system_clock::now();
        
        for (const auto& pair : snapshots_) {
            const auto& snapshot = pair.second;
            
            // Aggregate connection metrics
            aggregated.connection.attempts += snapshot.connection.attempts;
            aggregated.connection.successes += snapshot.connection.successes;
            aggregated.connection.failures += snapshot.connection.failures;
            aggregated.connection.reconnects += snapshot.connection.reconnects;
            aggregated.connection.totalLatency += snapshot.connection.totalLatency;
            
            // Aggregate publisher metrics
            aggregated.publisher.messagesPublished += snapshot.publisher.messagesPublished;
            aggregated.publisher.failures += snapshot.publisher.failures;
            aggregated.publisher.confirms += snapshot.publisher.confirms;
            aggregated.publisher.retries += snapshot.publisher.retries;
            aggregated.publisher.batchesPublished += snapshot.publisher.batchesPublished;
            aggregated.publisher.totalBatchSize += snapshot.publisher.totalBatchSize;
            aggregated.publisher.localQueueSize += snapshot.publisher.localQueueSize;
            aggregated.publisher.totalLatency += snapshot.publisher.totalLatency;
            
            // Aggregate consumer metrics
            aggregated.consumer.messagesConsumed += snapshot.consumer.messagesConsumed;
            aggregated.consumer.messagesAcknowledged += snapshot.consumer.messagesAcknowledged;
            aggregated.consumer.messagesRejected += snapshot.consumer.messagesRejected;
            aggregated.consumer.messagesRequeued += snapshot.consumer.messagesRequeued;
            aggregated.consumer.cancellations += snapshot.consumer.cancellations;
            aggregated.consumer.activeConsumers += snapshot.consumer.activeConsumers;
            aggregated.consumer.totalProcessingTime += snapshot.consumer.totalProcessingTime;
            
            // Aggregate queue metrics
            aggregated.queue.queuesCreated += snapshot.queue.queuesCreated;
            aggregated.queue.queuesDeleted += snapshot.queue.queuesDeleted;
            aggregated.queue.queuesPurged += snapshot.queue.queuesPurged;
            aggregated.queue.totalDepth += snapshot.queue.totalDepth;
            
            // Aggregate queue depths
            for (const auto& queuePair : snapshot.queue.queueDepths) {
                aggregated.queue.queueDepths[queuePair.first] += queuePair.second;
            }
            
            // Aggregate system metrics
            aggregated.system.memoryUsage += snapshot.system.memoryUsage;
            aggregated.system.cpuUsage += snapshot.system.cpuUsage;
            aggregated.system.errors += snapshot.system.errors;
        }
        
        // Average CPU usage
        if (!snapshots_.empty()) {
            aggregated.system.cpuUsage /= snapshots_.size();
        }
        
        return aggregated;
    }
    
    size_t getInstanceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }
    
    void removeInstance(const std::string& instanceId) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.erase(instanceId);
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, MetricsSnapshot> snapshots_;
};

/**
 * @brief Metrics reporter for generating human-readable reports
 */
class MetricsReporter {
public:
    static std::string generateTextReport(const MetricsSnapshot& snapshot) {
        std::ostringstream report;
        
        report << "RabbitMQ Integration Metrics Report\n";
        report << "===================================\n";
        report << "Timestamp: " << formatTimestamp(snapshot.timestamp) << "\n\n";
        
        // Connection metrics
        report << "Connection Metrics:\n";
        report << "  Attempts: " << snapshot.connection.attempts << "\n";
        report << "  Successes: " << snapshot.connection.successes << "\n";
        report << "  Failures: " << snapshot.connection.failures << "\n";
        report << "  Success Rate: " << calculatePercentage(snapshot.connection.successes, snapshot.connection.attempts) << "%\n";
        if (snapshot.connection.attempts > 0) {
            report << "  Avg Latency: " << (snapshot.connection.totalLatency.count() / snapshot.connection.attempts) << "ms\n";
        }
        report << "\n";
        
        // Publisher metrics
        report << "Publisher Metrics:\n";
        report << "  Messages Published: " << snapshot.publisher.messagesPublished << "\n";
        report << "  Publish Failures: " << snapshot.publisher.failures << "\n";
        report << "  Publish Confirms: " << snapshot.publisher.confirms << "\n";
        report << "  Publish Retries: " << snapshot.publisher.retries << "\n";
        report << "  Batches Published: " << snapshot.publisher.batchesPublished << "\n";
        report << "  Local Queue Size: " << snapshot.publisher.localQueueSize << "\n";
        report << "  Success Rate: " << calculatePercentage(snapshot.publisher.messagesPublished, 
                                                          snapshot.publisher.messagesPublished + snapshot.publisher.failures) << "%\n";
        if (snapshot.publisher.batchesPublished > 0) {
            report << "  Avg Batch Size: " << (snapshot.publisher.totalBatchSize / snapshot.publisher.batchesPublished) << "\n";
        }
        report << "\n";
        
        // Consumer metrics
        report << "Consumer Metrics:\n";
        report << "  Messages Consumed: " << snapshot.consumer.messagesConsumed << "\n";
        report << "  Messages Acknowledged: " << snapshot.consumer.messagesAcknowledged << "\n";
        report << "  Messages Rejected: " << snapshot.consumer.messagesRejected << "\n";
        report << "  Active Consumers: " << snapshot.consumer.activeConsumers << "\n";
        report << "  Ack Rate: " << calculatePercentage(snapshot.consumer.messagesAcknowledged, snapshot.consumer.messagesConsumed) << "%\n";
        if (snapshot.consumer.messagesConsumed > 0) {
            report << "  Avg Processing Time: " << (snapshot.consumer.totalProcessingTime.count() / snapshot.consumer.messagesConsumed) << "ms\n";
        }
        report << "\n";
        
        // Queue metrics
        report << "Queue Metrics:\n";
        report << "  Queues Created: " << snapshot.queue.queuesCreated << "\n";
        report << "  Queues Deleted: " << snapshot.queue.queuesDeleted << "\n";
        report << "  Queues Purged: " << snapshot.queue.queuesPurged << "\n";
        report << "  Total Queue Depth: " << snapshot.queue.totalDepth << "\n";
        report << "  Active Queues: " << snapshot.queue.queueDepths.size() << "\n";
        report << "\n";
        
        // System metrics
        report << "System Metrics:\n";
        report << "  Memory Usage: " << formatBytes(snapshot.system.memoryUsage) << "\n";
        report << "  CPU Usage: " << std::fixed << std::setprecision(1) << snapshot.system.cpuUsage << "%\n";
        report << "  Total Errors: " << snapshot.system.errors << "\n";
        
        return report.str();
    }

private:
    static std::string formatTimestamp(std::chrono::system_clock::time_point tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
        return ss.str();
    }
    
    static double calculatePercentage(uint64_t numerator, uint64_t denominator) {
        if (denominator == 0) return 0.0;
        return (double)numerator / denominator * 100.0;
    }
    
    static std::string formatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        size_t unit = 0;
        double size = bytes;
        
        while (size >= 1024 && unit < 4) {
            size /= 1024;
            unit++;
        }
        
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }
};

/**
 * @brief Metrics exporter for different formats
 */
class MetricsExporter {
public:
    explicit MetricsExporter(const MetricsCollector& collector) 
        : collector_(collector) {}
    
    // Export to Prometheus format
    std::string exportPrometheus() const {
        auto snapshot = collector_.getSnapshot();
        std::ostringstream prom;
        
        // Connection metrics
        prom << "# HELP rabbitmq_integration_connection_attempts_total Total connection attempts\n";
        prom << "# TYPE rabbitmq_integration_connection_attempts_total counter\n";
        prom << "rabbitmq_integration_connection_attempts_total " << snapshot.connection.attempts << "\n";
        
        // Publisher metrics
        prom << "# HELP rabbitmq_integration_messages_published_total Messages published\n";
        prom << "# TYPE rabbitmq_integration_messages_published_total counter\n";
        prom << "rabbitmq_integration_messages_published_total " << snapshot.publisher.messagesPublished << "\n";
        
        // Consumer metrics
        prom << "# HELP rabbitmq_integration_messages_consumed_total Messages consumed\n";
        prom << "# TYPE rabbitmq_integration_messages_consumed_total counter\n";
        prom << "rabbitmq_integration_messages_consumed_total " << snapshot.consumer.messagesConsumed << "\n";
        
        // System metrics
        prom << "# HELP rabbitmq_integration_memory_usage_bytes Memory usage in bytes\n";
        prom << "# TYPE rabbitmq_integration_memory_usage_bytes gauge\n";
        prom << "rabbitmq_integration_memory_usage_bytes " << snapshot.system.memoryUsage << "\n";
        
        return prom.str();
    }

private:
    const MetricsCollector& collector_;
};

/**
 * @brief Metrics alerting system
 */
class MetricsAlerter {
public:
    struct AlertRule {
        std::string name;
        std::string description;
        std::function<bool(const MetricsSnapshot&)> condition;
        std::chrono::seconds cooldown = std::chrono::seconds(300);
        std::string severity = "warning";
    };
    
    struct Alert {
        std::string name;
        std::string description;
        std::string severity;
        std::chrono::system_clock::time_point timestamp;
        MetricsSnapshot snapshot;
    };
    
    void addRule(const AlertRule& rule) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_.push_back(rule);
    }
    
    std::vector<Alert> checkAlerts(const MetricsSnapshot& snapshot) {
        std::vector<Alert> triggered;
        std::lock_guard<std::mutex> lock(rulesMutex_);
        
        auto now = std::chrono::system_clock::now();
        
        for (const auto& rule : rules_) {
            // Check cooldown
            auto lastAlert = lastAlerts_.find(rule.name);
            if (lastAlert != lastAlerts_.end() && 
                now - lastAlert->second < rule.cooldown) {
                continue;
            }
            
            // Check condition
            if (rule.condition(snapshot)) {
                Alert alert;
                alert.name = rule.name;
                alert.description = rule.description;
                alert.severity = rule.severity;
                alert.timestamp = now;
                alert.snapshot = snapshot;
                
                triggered.push_back(alert);
                lastAlerts_[rule.name] = now;
            }
        }
        
        return triggered;
    }
    
    void setupDefaultRules() {
        // High connection failure rate
        addRule({
            "high_connection_failures",
            "Connection failure rate is high",
            [](const MetricsSnapshot& s) {
                return s.connection.attempts > 10 && 
                       (double)s.connection.failures / s.connection.attempts > 0.2;
            },
            std::chrono::seconds(300),
            "critical"
        });
        
        // High publish failure rate
        addRule({
            "high_publish_failures",
            "Publish failure rate is high",
            [](const MetricsSnapshot& s) {
                auto total = s.publisher.messagesPublished + s.publisher.failures;
                return total > 100 && (double)s.publisher.failures / total > 0.05;
            },
            std::chrono::seconds(180),
            "warning"
        });
        
        // Large local queue
        addRule({
            "large_local_queue",
            "Local queue size is large",
            [](const MetricsSnapshot& s) {
                return s.publisher.localQueueSize > 10000;
            },
            std::chrono::seconds(600),
            "warning"
        });
        
        // High memory usage
        addRule({
            "high_memory_usage",
            "Memory usage is high",
            [](const MetricsSnapshot& s) {
                return s.system.memoryUsage > 1024 * 1024 * 1024; // 1GB
            },
            std::chrono::seconds(300),
            "warning"
        });
        
        // High CPU usage
        addRule({
            "high_cpu_usage",
            "CPU usage is high",
            [](const MetricsSnapshot& s) {
                return s.system.cpuUsage > 80.0;
            },
            std::chrono::seconds(300),
            "warning"
        });
    }

private:
    std::mutex rulesMutex_;
    std::vector<AlertRule> rules_;
    std::map<std::string, std::chrono::system_clock::time_point> lastAlerts_;
};

} // namespace metrics
} // namespace rabbitmq_integration