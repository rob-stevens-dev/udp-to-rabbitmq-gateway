#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <map>
#include <unordered_map>
#include <amqp.h>
#include "rabbitmq_integration/types.hpp"

namespace rabbitmq_integration {

// Forward declare enums from types.hpp
enum class DeliveryMode : uint8_t;
enum class Priority : uint8_t;

// Add this type alias at the top of the file (tests expect this):
using Properties = std::map<std::string, std::string>;

class Message {
public:
    // Constructors
    Message();  // Declare explicitly instead of using default
    // explicit Message(const nlohmann::json& data);
    Message(const nlohmann::json& data);
    Message(const uint8_t* data, size_t length);
    explicit Message(const std::string& data);
    Message(const std::vector<uint8_t>& data);
    
    // Copy and move constructors
    Message(const Message& other) = default;
    Message(Message&& other) noexcept = default;
    Message& operator=(const Message& other) = default;
    Message& operator=(Message&& other) noexcept = default;
    
    // Destructor
    ~Message() = default;

    // Content methods
    nlohmann::json toJson() const;
    std::vector<uint8_t> toBinary() const;
    std::string toString() const;
    void setContent(const nlohmann::json& data);
    void setContent(const std::string& data);
    void setContent(const std::vector<uint8_t>& data);
    void setContent(const uint8_t* data, size_t length);
    
    // Methods expected by tests
    size_t getContentLength() const;
    std::vector<uint8_t> getContentBytes() const;
    std::string getContentString() const;
    nlohmann::json getContentJson() const;
    bool isJson() const;
    bool isText() const;
    size_t estimateSize() const;

    // Static factory methods expected by tests
    static Message createJsonMessage(const nlohmann::json& data);
    static Message createTextMessage(const std::string& text);
    static Message createBinaryMessage(const std::vector<uint8_t>& data);

    // Message properties
    void setProperty(const std::string& name, const std::string& value);
    std::string getProperty(const std::string& name) const;
    bool hasProperty(const std::string& name) const;
    void removeProperty(const std::string& name);
    void clearProperties();
    
    // Standard AMQP properties
    void setContentType(const std::string& contentType);
    std::string getContentType() const;
    
    void setContentEncoding(const std::string& encoding);
    std::string getContentEncoding() const;
    
    void setMessageId(const std::string& id);
    std::string getMessageId() const;
    
    void setCorrelationId(const std::string& id);
    std::string getCorrelationId() const;
    
    void setReplyTo(const std::string& replyTo);
    std::string getReplyTo() const;
    
    void setExpiration(const std::string& expiration);
    std::chrono::milliseconds getExpiration() const;  // Changed from std::string
    std::string getExpirationString() const;          // Add this new methodgetExpiration() const;
    
    void setTimestamp(std::chrono::system_clock::time_point timestamp);
    std::chrono::system_clock::time_point getTimestamp() const;
    
    void setType(const std::string& type);
    std::string getType() const;
    
    void setUserId(const std::string& userId);
    std::string getUserId() const;
    
    void setAppId(const std::string& appId);
    std::string getAppId() const;

    // Data access methods that the implementation expects
    const std::vector<uint8_t>& getData() const;
    void setData(const std::vector<uint8_t>& data);
    void setData(const std::string& data);
    void setData(const nlohmann::json& data);
    void setData(const uint8_t* data, size_t length);
    
    // Properties access
    bool fromJson(const nlohmann::json& json);
    const MessageProperties& getProperties() const;
    MessageProperties& getProperties();
    void setProperties(const MessageProperties& properties);

    // Persistence methods
    void setPersistent(bool persistent);
    bool isPersistent() const;
    
    // Header methods
    std::string getHeader(const std::string& key) const;
    const std::map<std::string, std::string>& getHeaders() const;
    void setHeaders(const std::map<std::string, std::string>& headers);
    
    // Additional methods the implementation expects
    void setTimestampNow();
    std::optional<std::chrono::milliseconds> getExpirationDuration() const;
    void setExpiration(std::chrono::milliseconds ttl);
    uint8_t getPriorityValue() const;
    void setPriority(uint8_t priority);
    std::string toString(bool includeProperties) const;
    void dump() const;
    bool compress();
    bool decompress();
    bool isCompressed() const;
    std::string serialize() const;
    bool deserialize(const std::string& serialized);
    std::string calculateChecksum() const;
    bool verifyChecksum(const std::string& expectedChecksum) const;

    std::string getBody() const;

    // Delivery mode
    void setDeliveryMode(DeliveryMode mode);
    DeliveryMode getDeliveryMode() const;
    
    // Priority
    void setPriority(Priority priority);
    Priority getPriority() const;
    
    // TTL
    void setTTL(std::chrono::milliseconds ttl);
    std::chrono::milliseconds getTTL() const;
    
    // Size information
    size_t getSize() const;
    size_t size() const { return getSize(); } // Alias for compatibility
    bool isEmpty() const;
    bool empty() const { return isEmpty(); } // Alias for compatibility
    
    // Validation
    bool isValid() const;
    std::string validate() const;
    
    // AMQP conversion
    amqp_basic_properties_t toAmqpProperties() const;
    void fromAmqpProperties(const amqp_basic_properties_t& props);
    
    // Headers (for AMQP headers)
    void setHeader(const std::string& name, const std::string& value);
    void setHeader(const std::string& name, const char* value);
    void setHeader(const std::string& name, int64_t value);
    void setHeader(const std::string& name, double value);
    void setHeader(const std::string& name, bool value);
    std::optional<std::string> getHeaderString(const std::string& name) const;
    std::optional<int64_t> getHeaderInt(const std::string& name) const;
    std::optional<double> getHeaderDouble(const std::string& name) const;
    std::optional<bool> getHeaderBool(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
    void removeHeader(const std::string& name);
    void clearHeaders();
    
    // Utility methods
    void clear();
    Message clone() const;
    
    // Comparison operators
    bool operator==(const Message& other) const;
    bool operator!=(const Message& other) const;

private:
    std::vector<uint8_t> data_;
    MessageProperties properties_;
    std::vector<uint8_t> body_;
    std::unordered_map<std::string, std::string> headers_;
    
    // AMQP basic properties
    std::string contentType_{"application/octet-stream"};  // Changed from "application/json"
    std::string contentEncoding_;
    std::string messageId_;
    std::string correlationId_;
    std::string replyTo_;
    std::string expiration_;
    std::chrono::system_clock::time_point timestamp_;
    std::string type_;
    std::string userId_;
    std::string appId_;
    DeliveryMode deliveryMode_{DeliveryMode::NonPersistent};
    Priority priority_{Priority::Normal};
    
    // Helper methods
    void ensureTimestampSet();
    void ensureMessageIdSet();
    void updateTimestamp();
    bool isValidJson(const std::string& str) const;
    std::string generateMessageId() const;
};

// MessageUtils namespace that tests expect
namespace MessageUtils {
    Message createTelemetryMessage(const std::string& imei, double latitude, double longitude, 
                                  double speed = 0.0, double battery = 1.0, 
                                  const nlohmann::json& custom = nlohmann::json::object());
    
    Message createCommandMessage(const std::string& command, 
                                const nlohmann::json& params = nlohmann::json::object(),
                                const std::string& correlationId = "");
    
    Message createDebugMessage(const std::string& imei, const std::string& debugType, 
                              const nlohmann::json& debugData);
    
    Message createDiscardMessage(const std::string& imei, const std::string& reason,
                                const nlohmann::json& originalData);
    
    bool isValidTelemetryMessage(const Message& message);
    bool isValidCommandMessage(const Message& message);
    
    std::string extractImei(const Message& message);
    std::string generateCorrelationId();
    std::string generateMessageId();
}

} // namespace rabbitmq_integration