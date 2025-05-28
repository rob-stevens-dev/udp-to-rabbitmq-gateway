#pragma once

#include <functional>
#include <string>
#include <chrono>
#include <cstdint>
#include <amqp.h>
#include <atomic>
#include <optional>
#include <nlohmann/json.hpp>

namespace rabbitmq_integration {

// Forward declarations
class Connection;
class Channel;
class Message;

// Connection states
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Reconnecting,
    Error,
    Failed = Error  // Alias for backward compatibility
};

// Channel states
enum class ChannelState {
    Closed,
    Opening,
    Open,
    Closing,
    Failed
};

// Error types
enum class ErrorType {
    None,
    ConnectionError,
    ChannelError,
    AuthenticationError,
    NetworkError,
    ProtocolError,
    TimeoutError,
    ResourceError
};


// Result template for operations that can succeed or fail
template<typename T>
class Result {
public:
    // Success constructor
    explicit Result(T value) : success(true), value(std::move(value)), error(ErrorType::None) {}
    
    // Failure constructor
    explicit Result(ErrorType errorType, const std::string& errorMessage = "") 
        : success(false), error(errorType), message(errorMessage) {}
    
    // Default constructor (success for void)
    Result() : success(true), error(ErrorType::None) {}
    
    // Check if result is successful
    operator bool() const { return success; }
    
    // Access value (only if successful)
    T& operator*() { return value; }
    const T& operator*() const { return value; }
    
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
    
    bool success;
    T value{};
    ErrorType error{ErrorType::None};
    std::string message;
};

// Specialization for void
template<>
class Result<void> {
public:
    // Success constructor
    Result() : success(true), error(ErrorType::None) {}
    
    // Failure constructor
    explicit Result(ErrorType errorType, const std::string& errorMessage = "") 
        : success(false), error(errorType), message(errorMessage) {}
    
    // Check if result is successful
    operator bool() const { return success; }
    
    bool success;
    ErrorType error{ErrorType::None};
    std::string message;
};


// Message delivery modes
enum class DeliveryMode : uint8_t {
    NonPersistent = 1,
    Persistent = 2
};

// Message priorities
enum class Priority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Urgent = 3
};

// Connection configuration
struct ConnectionConfig {
    std::string host{"localhost"};
    int port{5672};
    std::string vhost{"/"};
    std::string username{"guest"};
    std::string password{"guest"};
    bool useTLS{false};
    
    // Connection settings
    uint16_t maxChannels{2047};
    //uint32_t frameMaxSize{131072};
    uint16_t heartbeatTimeout{60};
    std::chrono::seconds heartbeatInterval{60};
    
    // Connection pool settings
    std::chrono::milliseconds connectionTimeout{std::chrono::seconds(30)};
    std::chrono::milliseconds retryInterval{std::chrono::seconds(5)};
    int maxRetries{3};
    
    // TLS settings
    std::string caCertPath;
    std::string clientCertPath;
    std::string clientKeyPath;
    bool verifyPeer{true};
    bool verifyHostname{true};

    // ADD THESE MISSING FIELDS:
    std::chrono::seconds heartbeat{60};
    bool autoReconnect{true};
    uint32_t frameMax{131072};  // Note: test expects frameMax, not frameMaxSize
    uint32_t channelMax{0};
};

// Publisher configuration
struct PublisherConfig : public ConnectionConfig {
    bool confirmEnabled{true};
    bool persistentMessages{true};
    bool useTransactions{false};
    size_t localQueueSize{10000};
    std::chrono::milliseconds publishTimeout{std::chrono::seconds(30)};
    int batchSize{100};
    bool enableBatching{false};

    // ADD THESE 3 MISSING FIELDS:
    std::chrono::seconds confirmTimeout{30};       // Missing field
    bool dropOnOverflow{false};                    // Missing field  
    std::chrono::milliseconds batchTimeout{100};   // Test expects batchTimeout not publishTimeout
};

// Consumer configuration
struct ConsumerConfig : public ConnectionConfig {
    uint16_t prefetchCount{10};
    bool autoAck{false};
    bool exclusive{false};
    std::chrono::milliseconds ackTimeout{std::chrono::seconds(30)};
    std::chrono::milliseconds consumeTimeout{std::chrono::seconds(30)};
    bool enableRetry{true};
    int maxRetryAttempts{3};
    bool autoRecover{false};

    // ADD THESE 3 MISSING FIELDS:
    bool noLocal{false};                           // Missing field
    bool autoRestart{true};                        // Missing field
    std::chrono::milliseconds restartDelay{1000};   // Missing field that tests expect
};

// Connection statistics
struct ConnectionStats {
    uint64_t messagesReceived{0};
    uint64_t messagesSent{0};
    std::atomic<uint64_t> bytesReceived{0};         // uint64_t bytesReceived{0};
    std::atomic<uint64_t> bytesSent{0};             // uint64_t bytesSent{0};
    std::atomic<uint64_t> connectAttempts{0};       // uint64_t connectAttempts{0};
    uint64_t reconnectAttempts{0};
    uint64_t channelsCreated{0};
    uint64_t channelsClosed{0};
    uint64_t channelCount{0};  // Current number of channels
    std::chrono::system_clock::time_point connectionStarted;
    std::chrono::system_clock::time_point lastActivity;
    std::chrono::system_clock::time_point connectedAt;
    std::string connectionId;
    ConnectionState state{ConnectionState::Disconnected};

    // ADD THESE 4 MISSING ATOMIC FIELDS:
    std::atomic<uint64_t> successfulConnects{0};   // Missing field
    std::atomic<uint64_t> failedConnects{0};       // Missing field
    std::atomic<uint64_t> disconnects{0};          // Missing field
    std::atomic<uint64_t> reconnects{0};           // Missing field

        // Custom constructors and assignment operators for atomic handling
    ConnectionStats() = default;
    
    ConnectionStats(const ConnectionStats& other) :
        messagesReceived(other.messagesReceived),
        messagesSent(other.messagesSent),
        bytesReceived(other.bytesReceived.load()),
        bytesSent(other.bytesSent.load()),
        connectAttempts(other.connectAttempts.load()),
        reconnectAttempts(other.reconnectAttempts),
        channelsCreated(other.channelsCreated),
        channelsClosed(other.channelsClosed),
        channelCount(other.channelCount),
        connectionStarted(other.connectionStarted),
        lastActivity(other.lastActivity),
        connectedAt(other.connectedAt),
        connectionId(other.connectionId),
        state(other.state),
        successfulConnects(other.successfulConnects.load()),
        failedConnects(other.failedConnects.load()),
        disconnects(other.disconnects.load()),
        reconnects(other.reconnects.load()) {}
    
    ConnectionStats(ConnectionStats&& other) noexcept :
        messagesReceived(other.messagesReceived),
        messagesSent(other.messagesSent),
        bytesReceived(other.bytesReceived.load()),
        bytesSent(other.bytesSent.load()),
        connectAttempts(other.connectAttempts.load()),
        reconnectAttempts(other.reconnectAttempts),
        channelsCreated(other.channelsCreated),
        channelsClosed(other.channelsClosed),
        channelCount(other.channelCount),
        connectionStarted(std::move(other.connectionStarted)),
        lastActivity(std::move(other.lastActivity)),
        connectedAt(std::move(other.connectedAt)),
        connectionId(std::move(other.connectionId)),
        state(other.state),
        successfulConnects(other.successfulConnects.load()),
        failedConnects(other.failedConnects.load()),
        disconnects(other.disconnects.load()),
        reconnects(other.reconnects.load()) {}
    
    ConnectionStats& operator=(const ConnectionStats& other) {
        if (this != &other) {
            messagesReceived = other.messagesReceived;
            messagesSent = other.messagesSent;
            bytesReceived.store(other.bytesReceived.load());
            bytesSent.store(other.bytesSent.load());
            connectAttempts.store(other.connectAttempts.load());
            reconnectAttempts = other.reconnectAttempts;
            channelsCreated = other.channelsCreated;
            channelsClosed = other.channelsClosed;
            channelCount = other.channelCount;
            connectionStarted = other.connectionStarted;
            lastActivity = other.lastActivity;
            connectedAt = other.connectedAt;
            connectionId = other.connectionId;
            state = other.state;
            successfulConnects.store(other.successfulConnects.load());
            failedConnects.store(other.failedConnects.load());
            disconnects.store(other.disconnects.load());
            reconnects.store(other.reconnects.load());
        }
        return *this;
    }
    
    ConnectionStats& operator=(ConnectionStats&& other) noexcept {
        if (this != &other) {
            messagesReceived = other.messagesReceived;
            messagesSent = other.messagesSent;
            bytesReceived.store(other.bytesReceived.load());
            bytesSent.store(other.bytesSent.load());
            connectAttempts.store(other.connectAttempts.load());
            reconnectAttempts = other.reconnectAttempts;
            channelsCreated = other.channelsCreated;
            channelsClosed = other.channelsClosed;
            channelCount = other.channelCount;
            connectionStarted = std::move(other.connectionStarted);
            lastActivity = std::move(other.lastActivity);
            connectedAt = std::move(other.connectedAt);
            connectionId = std::move(other.connectionId);
            state = other.state;
            successfulConnects.store(other.successfulConnects.load());
            failedConnects.store(other.failedConnects.load());
            disconnects.store(other.disconnects.load());
            reconnects.store(other.reconnects.load());
        }
        return *this;
    }
};

// Publisher statistics
struct PublisherStats {
    std::atomic<uint64_t> messagesPublished{0};     // uint64_t messagesPublished{0};
    std::atomic<uint64_t> publishConfirms{0};       // uint64_t publishConfirms{0};
    std::atomic<uint64_t> publishFailed{0};         // uint64_t publishFailed{0};
    uint64_t messagesConfirmed{0};
    uint64_t messagesNacked{0};
    uint64_t messagesReturned{0};
    std::atomic<uint64_t> localQueueSize{0};        // uint64_t localQueueSize{0};
    uint64_t bytesPublished{0};
    uint64_t transactionCount{0};
    uint64_t transactionRollbacks{0};
    uint64_t messagesFailed{0};
    uint64_t localQueueMaxSize{0};
    std::chrono::milliseconds maxPublishTime{0};
    std::chrono::milliseconds avgPublishTime{0};
    std::chrono::system_clock::time_point lastPublish;
    std::string publisherId;

    // ADD THESE 2 MISSING ATOMIC FIELDS:
    std::atomic<uint64_t> publishRetries{0};       // Missing field - tests expect publishRetries not reconnectAttempts
    std::atomic<uint64_t> messagesDropped{0};      // Missing field - tests expect messagesDropped not messagesNacked

        // Custom constructors and assignment operators for atomic handling
    PublisherStats() = default;
    
    PublisherStats(const PublisherStats& other) :
        messagesPublished(other.messagesPublished.load()),
        publishConfirms(other.publishConfirms.load()),
        publishFailed(other.publishFailed.load()),
        messagesConfirmed(other.messagesConfirmed),
        messagesNacked(other.messagesNacked),
        messagesReturned(other.messagesReturned),
        localQueueSize(other.localQueueSize.load()),
        bytesPublished(other.bytesPublished),
        transactionCount(other.transactionCount),
        transactionRollbacks(other.transactionRollbacks),
        messagesFailed(other.messagesFailed),
        localQueueMaxSize(other.localQueueMaxSize),
        maxPublishTime(other.maxPublishTime),
        avgPublishTime(other.avgPublishTime),
        lastPublish(other.lastPublish),
        publisherId(other.publisherId),
        publishRetries(other.publishRetries.load()),
        messagesDropped(other.messagesDropped.load()) {}
    
    PublisherStats(PublisherStats&& other) noexcept :
        messagesPublished(other.messagesPublished.load()),
        publishConfirms(other.publishConfirms.load()),
        publishFailed(other.publishFailed.load()),
        messagesConfirmed(other.messagesConfirmed),
        messagesNacked(other.messagesNacked),
        messagesReturned(other.messagesReturned),
        localQueueSize(other.localQueueSize.load()),
        bytesPublished(other.bytesPublished),
        transactionCount(other.transactionCount),
        transactionRollbacks(other.transactionRollbacks),
        messagesFailed(other.messagesFailed),
        localQueueMaxSize(other.localQueueMaxSize),
        maxPublishTime(other.maxPublishTime),
        avgPublishTime(other.avgPublishTime),
        lastPublish(std::move(other.lastPublish)),
        publisherId(std::move(other.publisherId)),
        publishRetries(other.publishRetries.load()),
        messagesDropped(other.messagesDropped.load()) {}
    
    PublisherStats& operator=(const PublisherStats& other) {
        if (this != &other) {
            messagesPublished.store(other.messagesPublished.load());
            publishConfirms.store(other.publishConfirms.load());
            publishFailed.store(other.publishFailed.load());
            messagesConfirmed = other.messagesConfirmed;
            messagesNacked = other.messagesNacked;
            messagesReturned = other.messagesReturned;
            localQueueSize.store(other.localQueueSize.load());
            bytesPublished = other.bytesPublished;
            transactionCount = other.transactionCount;
            transactionRollbacks = other.transactionRollbacks;
            messagesFailed = other.messagesFailed;
            localQueueMaxSize = other.localQueueMaxSize;
            maxPublishTime = other.maxPublishTime;
            avgPublishTime = other.avgPublishTime;
            lastPublish = other.lastPublish;
            publisherId = other.publisherId;
            publishRetries.store(other.publishRetries.load());
            messagesDropped.store(other.messagesDropped.load());
        }
        return *this;
    }
    
    PublisherStats& operator=(PublisherStats&& other) noexcept {
        if (this != &other) {
            messagesPublished.store(other.messagesPublished.load());
            publishConfirms.store(other.publishConfirms.load());
            publishFailed.store(other.publishFailed.load());
            messagesConfirmed = other.messagesConfirmed;
            messagesNacked = other.messagesNacked;
            messagesReturned = other.messagesReturned;
            localQueueSize.store(other.localQueueSize.load());
            bytesPublished = other.bytesPublished;
            transactionCount = other.transactionCount;
            transactionRollbacks = other.transactionRollbacks;
            messagesFailed = other.messagesFailed;
            localQueueMaxSize = other.localQueueMaxSize;
            maxPublishTime = other.maxPublishTime;
            avgPublishTime = other.avgPublishTime;
            lastPublish = std::move(other.lastPublish);
            publisherId = std::move(other.publisherId);
            publishRetries.store(other.publishRetries.load());
            messagesDropped.store(other.messagesDropped.load());
        }
        return *this;
    }
};

// Consumer statistics
struct ConsumerStats {
    std::atomic<uint64_t> messagesReceived{0};      // uint64_t messagesReceived{0};
    std::atomic<uint64_t> messagesAcknowledged{0};  // uint64_t messagesAcknowledged{0};
    std::atomic<uint64_t> messagesRejected{0};      // uint64_t messagesRejected{0};
    std::atomic<uint64_t> messagesRequeued{0};      // uint64_t messagesRequeued{0};
    uint64_t messagesUnacked{0};
    uint64_t reconnectAttempts{0};
    uint64_t bytesReceived{0};
    std::chrono::milliseconds maxProcessingTime{0};
    std::chrono::milliseconds avgProcessingTime{0};
    std::chrono::system_clock::time_point lastMessage;
    std::chrono::system_clock::time_point consumerStarted;
    std::string consumerId;

    // ADD THESE 2 MISSING ATOMIC FIELDS:
    std::atomic<uint64_t> consumerCancellations{0}; // Missing field
    std::atomic<uint64_t> processingErrors{0};      // Missing field

        // Custom constructors and assignment operators for atomic handling
    ConsumerStats() = default;
    
    ConsumerStats(const ConsumerStats& other) :
        messagesReceived(other.messagesReceived.load()),
        messagesAcknowledged(other.messagesAcknowledged.load()),
        messagesRejected(other.messagesRejected.load()),
        messagesRequeued(other.messagesRequeued.load()),
        messagesUnacked(other.messagesUnacked),
        reconnectAttempts(other.reconnectAttempts),
        bytesReceived(other.bytesReceived),
        maxProcessingTime(other.maxProcessingTime),
        avgProcessingTime(other.avgProcessingTime),
        lastMessage(other.lastMessage),
        consumerStarted(other.consumerStarted),
        consumerId(other.consumerId),
        consumerCancellations(other.consumerCancellations.load()),
        processingErrors(other.processingErrors.load()) {}
    
    ConsumerStats(ConsumerStats&& other) noexcept :
        messagesReceived(other.messagesReceived.load()),
        messagesAcknowledged(other.messagesAcknowledged.load()),
        messagesRejected(other.messagesRejected.load()),
        messagesRequeued(other.messagesRequeued.load()),
        messagesUnacked(other.messagesUnacked),
        reconnectAttempts(other.reconnectAttempts),
        bytesReceived(other.bytesReceived),
        maxProcessingTime(other.maxProcessingTime),
        avgProcessingTime(other.avgProcessingTime),
        lastMessage(std::move(other.lastMessage)),
        consumerStarted(std::move(other.consumerStarted)),
        consumerId(std::move(other.consumerId)),
        consumerCancellations(other.consumerCancellations.load()),
        processingErrors(other.processingErrors.load()) {}
    
    ConsumerStats& operator=(const ConsumerStats& other) {
        if (this != &other) {
            messagesReceived.store(other.messagesReceived.load());
            messagesAcknowledged.store(other.messagesAcknowledged.load());
            messagesRejected.store(other.messagesRejected.load());
            messagesRequeued.store(other.messagesRequeued.load());
            messagesUnacked = other.messagesUnacked;
            reconnectAttempts = other.reconnectAttempts;
            bytesReceived = other.bytesReceived;
            maxProcessingTime = other.maxProcessingTime;
            avgProcessingTime = other.avgProcessingTime;
            lastMessage = other.lastMessage;
            consumerStarted = other.consumerStarted;
            consumerId = other.consumerId;
            consumerCancellations.store(other.consumerCancellations.load());
            processingErrors.store(other.processingErrors.load());
        }
        return *this;
    }
    
    ConsumerStats& operator=(ConsumerStats&& other) noexcept {
        if (this != &other) {
            messagesReceived.store(other.messagesReceived.load());
            messagesAcknowledged.store(other.messagesAcknowledged.load());
            messagesRejected.store(other.messagesRejected.load());
            messagesRequeued.store(other.messagesRequeued.load());
            messagesUnacked = other.messagesUnacked;
            reconnectAttempts = other.reconnectAttempts;
            bytesReceived = other.bytesReceived;
            maxProcessingTime = other.maxProcessingTime;
            avgProcessingTime = other.avgProcessingTime;
            lastMessage = std::move(other.lastMessage);
            consumerStarted = std::move(other.consumerStarted);
            consumerId = std::move(other.consumerId);
            consumerCancellations.store(other.consumerCancellations.load());
            processingErrors.store(other.processingErrors.load());
        }
        return *this;
    }
};

// Queue information
struct QueueInfo {
    std::string name;
    uint64_t messageCount{0};
    uint64_t consumerCount{0};
    bool durable{false};
    bool exclusive{false};
    bool autoDelete{false};
};

// Channel information
struct ChannelInfo {
    int channelId{0};
    bool isOpen{false};
    bool confirmsEnabled{false};
    bool transactionMode{false};
    uint64_t nextPublishSeqNo{1};
    std::chrono::system_clock::time_point lastActivity;
};

// Queue configuration
struct QueueConfig {
    bool durable{true};
    bool exclusive{false};
    bool autoDelete{false};
    std::map<std::string, std::string> arguments;
    std::optional<uint32_t> maxLength;                      // uint32_t maxLength{0};
    std::optional<std::chrono::milliseconds> messageTTL;    // std::chrono::milliseconds messageTTL{0};
    std::string deadLetterExchange;
    std::string deadLetterRoutingKey;

    std::string name;                                       // Tests expect this field
    std::optional<std::chrono::milliseconds> expires;       // ADD this missing field
    std::optional<uint32_t> maxLengthBytes;                 // ADD this missing field
};

// Exchange configuration
struct ExchangeConfig {
    std::string name;                                       // Tests expect this field
    bool internal{false};                                   // Tests expect this field
    std::string type{"direct"};
    bool durable{true};
    bool autoDelete{false};
    std::map<std::string, std::string> arguments;
};

// Binding configuration
struct BindingConfig {
    std::string queue;
    std::string exchange;
    std::string routingKey;
    std::map<std::string, std::string> arguments;
};

// Queue statistics
struct QueueStats {
    std::string name;
    uint64_t messageCount{0};
    uint64_t consumerCount{0};
    uint64_t messagesReady{0};
    uint64_t messagesUnacknowledged{0};
    double messageRate{0.0};
    std::chrono::system_clock::time_point lastActivity;
};

// Regional configuration
struct RegionalConfig {
    std::string region;
    std::vector<std::string> allowedRegions;
    bool enforceDataResidency{true};
    std::string defaultRegion{"na"};
};

// Message properties (for AMQP)
// struct MessageProperties {
//     std::string contentType;
//     std::string contentEncoding;
//     std::map<std::string, std::string> headers;
//     DeliveryMode deliveryMode{DeliveryMode::NonPersistent};
//     Priority priority{Priority::Normal};
//     std::string correlationId;
//     std::string replyTo;
//     std::string expiration;
//     std::string messageId;
//     std::chrono::system_clock::time_point timestamp;
//     std::string type;
//     std::string userId;
//     std::string appId;
// };

struct MessageProperties {
    std::string contentType = "application/json";
    std::string contentEncoding;
    std::string messageId;
    std::string correlationId;
    std::string replyTo;
    std::string expiration;
    std::string userId;
    std::string appId;
    std::string type;
    DeliveryMode deliveryMode = DeliveryMode::NonPersistent;
    Priority priority = Priority::Normal;
    uint8_t priorityValue = 1;  // Add this line
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> headers;
};

// Queue type enum
enum class QueueType {
    Classic,
    Quorum,
    Stream
};

// Message envelope (for received messages) - using shared_ptr to avoid incomplete type
struct MessageEnvelope {
    std::shared_ptr<Message> message;
    uint64_t deliveryTag{0};
    std::string exchange;
    std::string routingKey;
    bool redelivered{false};
    std::chrono::system_clock::time_point timestamp;
};

// Callback function types
using ErrorCallback = std::function<void(const std::string& errorCode, const std::string& message, const std::string& context)>;
using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;
using ConfirmCallback = std::function<void(uint64_t deliveryTag, bool ack)>;
using ReturnCallback = std::function<void(const Message& message, const std::string& reason)>;
using MessageCallback = std::function<bool(const std::string& consumerTag, const std::string& queueName, const Message& message, uint64_t deliveryTag)>;

// Exchange types enum
enum class ExchangeType {
    Direct,
    Topic,
    Fanout,
    Headers
};

// Exchange type string constants  
namespace ExchangeTypeStrings {
    constexpr const char* DIRECT = "direct";
    constexpr const char* TOPIC = "topic";
    constexpr const char* FANOUT = "fanout";
    constexpr const char* HEADERS = "headers";
}

// Queue naming patterns
namespace QueueNames {
    constexpr const char* INBOUND_PATTERN = "{region}.{imei}_inbound";
    constexpr const char* OUTBOUND_PATTERN = "{region}.{imei}_outbound";
    constexpr const char* DEBUG_PATTERN = "{region}.{imei}_debug";
    constexpr const char* DISCARD_PATTERN = "{region}.{imei}_discard";
}

// Exchange names
namespace ExchangeNames {
    constexpr const char* DEVICE_EXCHANGE = "device.exchange";
    constexpr const char* COMMAND_EXCHANGE = "command.exchange";
    constexpr const char* DEBUG_EXCHANGE = "debug.exchange";
}

// Error codes
namespace ErrorCodes {
    constexpr const char* CONNECTION_FAILED = "CONNECTION_FAILED";
    constexpr const char* CHANNEL_ERROR = "CHANNEL_ERROR";
    constexpr const char* PUBLISH_FAILED = "PUBLISH_FAILED";
    constexpr const char* CONSUME_FAILED = "CONSUME_FAILED";
    constexpr const char* QUEUE_DECLARE_FAILED = "QUEUE_DECLARE_FAILED";
    constexpr const char* EXCHANGE_DECLARE_FAILED = "EXCHANGE_DECLARE_FAILED";
    constexpr const char* AUTHENTICATION_FAILED = "AUTHENTICATION_FAILED";
    constexpr const char* TIMEOUT = "TIMEOUT";
}

// Forward declarations for factory classes
class ConnectionFactory;
class ConsumerFactory;
class PublisherFactory;

// Exception classes
class RabbitMQException : public std::exception {
public:
    explicit RabbitMQException(const std::string& message, ErrorType type = ErrorType::None);
    const char* what() const noexcept override;
    ErrorType getErrorType() const noexcept;

private:
    std::string message_;
    ErrorType errorType_;
};

class ConnectionException : public RabbitMQException {
public:
    explicit ConnectionException(const std::string& message);
};

class ChannelException : public RabbitMQException {
public:
    explicit ChannelException(const std::string& message);
};

class AuthenticationException : public RabbitMQException {
public:
    explicit AuthenticationException(const std::string& message);
};

class NetworkException : public RabbitMQException {
public:
    explicit NetworkException(const std::string& message);
};

class TimeoutException : public RabbitMQException {
public:
    explicit TimeoutException(const std::string& message);
};

// Utility function declarations
std::string exchangeTypeToString(ExchangeType type);
ExchangeType stringToExchangeType(const std::string& str);
std::string connectionStateToString(ConnectionState state);
std::string channelStateToString(ChannelState state);
std::string errorTypeToString(ErrorType type);
std::string amqpErrorToString(int amqpStatus);
ErrorType amqpErrorToErrorType(int amqpStatus);

// AMQP utility functions
amqp_table_t stringMapToAmqpTable(const std::map<std::string, std::string>& map);
std::map<std::string, std::string> amqpTableToStringMap(const amqp_table_t& table);

} // namespace rabbitmq_integration