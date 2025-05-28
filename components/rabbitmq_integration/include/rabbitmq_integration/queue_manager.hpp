#pragma once

#include "types.hpp"
#include "connection.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace rabbitmq_integration {

// Forward declaration
class Channel;

class QueueManager {
public:
    // Constructor
    explicit QueueManager(const ConnectionConfig& config);
    
    // Alternative constructor with connection
    explicit QueueManager(std::shared_ptr<Connection> connection);
    
    // Destructor
    ~QueueManager();
    
    // Non-copyable but movable
    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;
    QueueManager(QueueManager&& other) noexcept;
    QueueManager& operator=(QueueManager&& other) noexcept;
    
    // Device queue management
    bool declareDeviceQueues(const std::string& region, const std::string& imei);
    bool deleteDeviceQueues(const std::string& region, const std::string& imei);
    bool purgeDeviceQueues(const std::string& region, const std::string& imei);
    
    // Generic queue operations
    bool declareQueue(const std::string& queueName, const QueueConfig& config = {});
    bool deleteQueue(const std::string& queueName, bool ifUnused = false, bool ifEmpty = false);
    bool purgeQueue(const std::string& queueName);
    bool bindQueue(const std::string& queueName, const std::string& exchangeName,
                   const std::string& routingKey, const std::map<std::string, std::string>& arguments = {});
    bool unbindQueue(const std::string& queueName, const std::string& exchangeName,
                     const std::string& routingKey, const std::map<std::string, std::string>& arguments = {});
    
    // Exchange operations
    bool declareExchange(const std::string& exchangeName, const ExchangeConfig& config = {});
    bool deleteExchange(const std::string& exchangeName, bool ifUnused = false);
    
    // Queue inspection
    bool queueExists(const std::string& queueName);
    QueueStats getQueueInfo(const std::string& queueName);
    std::vector<std::string> listQueues(const std::string& pattern = "");
    
    // Exchange inspection
    bool exchangeExists(const std::string& exchangeName);
    std::vector<std::string> listExchanges(const std::string& pattern = "");
    
    // Binding inspection
    std::vector<BindingConfig> getQueueBindings(const std::string& queueName);
    std::vector<BindingConfig> getExchangeBindings(const std::string& exchangeName);
    
    // Bulk operations
    bool declareQueues(const std::vector<std::pair<std::string, QueueConfig>>& queues);
    bool deleteQueues(const std::vector<std::string>& queueNames);
    bool declareExchanges(const std::vector<std::pair<std::string, ExchangeConfig>>& exchanges);
    
    // Device management helpers
    std::vector<std::string> getDeviceQueues(const std::string& region, const std::string& imei);
    bool deviceQueuesExist(const std::string& region, const std::string& imei);
    
    // Regional queue management
    bool setupRegionalInfrastructure(const std::string& region);
    bool teardownRegionalInfrastructure(const std::string& region);
    std::vector<std::string> getRegionalQueues(const std::string& region);
    
    // Queue templates
    bool declareFromTemplate(const std::string& templateName, const std::string& queueName,
                           const std::map<std::string, std::string>& parameters = {});
    bool registerTemplate(const std::string& templateName, const QueueConfig& templateConfig);
    
    // Connection management
    bool isConnected() const;
    ConnectionState getConnectionState() const;
    bool reconnect();
    
    // Statistics
    std::map<std::string, QueueStats> getAllQueueStats();
    size_t getTotalQueues() const;
    size_t getTotalExchanges() const;
    
    // Configuration
    const ConnectionConfig& getConfig() const;
    
    // Error handling
    std::string getLastError() const;
    void setErrorCallback(ErrorCallback callback);
    
    // Health check
    bool performHealthCheck();
    
private:
    // Configuration
    ConnectionConfig config_;
    
    // Connection management
    std::shared_ptr<Connection> connection_;
    std::shared_ptr<Channel> channel_;
    bool ownsConnection_;
    
    // State
    mutable std::mutex mutex_;
    std::string lastError_;
    ErrorCallback errorCallback_;
    
    // Queue templates
    std::map<std::string, QueueConfig> queueTemplates_;
    
    // Cache for performance
    mutable std::mutex cacheMutex_;
    std::map<std::string, bool> queueExistsCache_;
    std::map<std::string, bool> exchangeExistsCache_;
    std::chrono::system_clock::time_point lastCacheUpdate_;
    std::chrono::minutes cacheExpiry_{5};
    
    // Private methods
    bool initializeConnection();
    void teardownConnection();
    bool initializeChannel();
    void teardownChannel();
    
    // Queue name generation
    std::string generateQueueName(const std::string& region, const std::string& imei,
                                const std::string& queueType) const;
    
    // Device queue operations
    bool declareDeviceQueue(const std::string& region, const std::string& imei,
                          const std::string& queueType);
    bool deleteDeviceQueue(const std::string& region, const std::string& imei,
                         const std::string& queueType);
    
    // Regional infrastructure
    bool declareRegionalExchanges(const std::string& region);
    bool deleteRegionalExchanges(const std::string& region);
    
    // Cache management
    void clearCache();
    bool isCacheValid() const;
    void updateCache();
    
    // Error handling
    void setError(const std::string& error);
    void handleAmqpError(const std::string& context);
    
    // Validation
    bool validateQueueName(const std::string& queueName) const;
    bool validateExchangeName(const std::string& exchangeName) const;
    bool validateRegion(const std::string& region) const;
    bool validateImei(const std::string& imei) const;
    
    // Template processing
    QueueConfig processTemplate(const QueueConfig& templateConfig,
                              const std::map<std::string, std::string>& parameters) const;
    std::string replaceParameters(const std::string& input,
                                const std::map<std::string, std::string>& parameters) const;
    
    // Default configurations
    QueueConfig getDefaultQueueConfig(const std::string& queueType) const;
    ExchangeConfig getDefaultExchangeConfig(const std::string& exchangeType) const;
    
    // Utility methods
    static std::string queueTypeToString(QueueType type);
    static std::string exchangeTypeToString(rabbitmq_integration::ExchangeType type);
    static QueueType stringToQueueType(const std::string& type);
    static rabbitmq_integration::ExchangeType stringToExchangeType(const std::string& type);
};

// Queue manager factory for creating specialized queue managers
class QueueManagerFactory {
public:
    // Create standard queue manager
    static std::unique_ptr<QueueManager> create(const ConnectionConfig& config);
    
    // Create queue manager with shared connection
    static std::unique_ptr<QueueManager> create(std::shared_ptr<Connection> connection);
    
    // Create regional queue manager
    static std::unique_ptr<QueueManager> createRegional(const ConnectionConfig& config,
                                                      const std::string& region);
    
    // Create queue manager with templates
    static std::unique_ptr<QueueManager> createWithTemplates(const ConnectionConfig& config,
                                                           const std::map<std::string, QueueConfig>& templates);
};

// Regional queue manager for handling region-specific operations
class RegionalQueueManager {
public:
    explicit RegionalQueueManager(const ConnectionConfig& config, const RegionalConfig& regionalConfig);
    ~RegionalQueueManager();
    
    // Non-copyable
    RegionalQueueManager(const RegionalQueueManager&) = delete;
    RegionalQueueManager& operator=(const RegionalQueueManager&) = delete;
    
    // Regional operations
    bool setupRegion();
    bool teardownRegion();
    bool isRegionSetup() const;
    
    // Device management
    bool addDevice(const std::string& imei);
    bool removeDevice(const std::string& imei);
    bool deviceExists(const std::string& imei) const;
    std::vector<std::string> getDevices() const;
    
    // Queue operations with regional compliance
    bool declareDeviceQueues(const std::string& imei);
    bool deleteDeviceQueues(const std::string& imei);
    
    // Data residency compliance
    bool enforceDataResidency() const;
    bool isDataResidencyCompliant(const std::string& queueName) const;
    
    // GDPR compliance (for EU region)
    bool enableGdprCompliance();
    bool isGdprCompliant() const;
    bool anonymizeDeviceData(const std::string& imei);
    
    // Statistics
    std::map<std::string, QueueStats> getRegionalStats() const;
    size_t getDeviceCount() const;
    
    // Configuration
    const RegionalConfig& getRegionalConfig() const;
    bool updateRegionalConfig(const RegionalConfig& config);
    
private:
    ConnectionConfig connectionConfig_;
    RegionalConfig regionalConfig_;
    std::unique_ptr<QueueManager> queueManager_;
    
    // Regional state
    std::atomic<bool> regionSetup_;
    std::set<std::string> registeredDevices_;
    mutable std::mutex devicesMutex_;
    
    // Private methods
    std::string getRegionalPrefix() const;
    bool setupRegionalExchanges();
    bool setupRegionalQueues();
    bool validateRegionalCompliance(const std::string& queueName) const;
    
    // GDPR specific methods
    void setupGdprCompliance();
    bool scheduleDataCleanup(const std::string& imei);
};

// Queue template manager for managing reusable queue configurations
class QueueTemplateManager {
public:
    QueueTemplateManager() = default;
    ~QueueTemplateManager() = default;
    
    // Template management
    bool registerTemplate(const std::string& name, const QueueConfig& config);
    bool unregisterTemplate(const std::string& name);
    bool hasTemplate(const std::string& name) const;
    
    QueueConfig getTemplate(const std::string& name) const;
    std::vector<std::string> getTemplateNames() const;
    
    // Template instantiation
    QueueConfig instantiateTemplate(const std::string& name,
                                  const std::map<std::string, std::string>& parameters) const;
    
    // Predefined templates
    void loadDefaultTemplates();
    void loadDeviceTemplates();
    void loadRegionalTemplates();
    
    // Template validation
    bool validateTemplate(const QueueConfig& config) const;
    bool validateParameters(const std::string& templateName,
                          const std::map<std::string, std::string>& parameters) const;
    
private:
    std::map<std::string, QueueConfig> templates_;
    mutable std::mutex templatesMutex_;
    
    // Parameter replacement
    std::string replaceParameters(const std::string& input,
                                const std::map<std::string, std::string>& parameters) const;
    
    // Template processing helpers
    QueueConfig processTemplateConfig(const QueueConfig& config,
                                    const std::map<std::string, std::string>& parameters) const;
    std::map<std::string, std::string> extractParametersFromTemplate(const QueueConfig& config) const;
};

} // namespace rabbitmq_integration