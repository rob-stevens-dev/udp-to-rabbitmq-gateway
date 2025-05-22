#pragma once

#include "protocol_parser/types.hpp"
#include "protocol_parser/error.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <chrono>
#include <optional>
#include <cstdint>

namespace protocol_parser {

// Forward declarations
class Parser;
class CommandBuilder;

/**
 * @brief Represents the parsed content of a protocol message
 * 
 * This class encapsulates all the data extracted from a binary protocol packet,
 * including header information, payload fields, and validation status.
 */
class Message {
public:
    // Value type that can hold various field types including all integer sizes
    using Value = std::variant<
        std::monostate,  // For unset/null values
        bool,
        int8_t,
        uint8_t,
        int16_t,
        uint16_t,
        int32_t,
        uint32_t,
        int64_t,
        uint64_t,
        float,
        double,
        std::string,
        std::vector<uint8_t>  // For binary data
    >;
    
    using FieldMap = std::unordered_map<std::string, Value>;
    using Timestamp = std::chrono::system_clock::time_point;

    /**
     * @brief Default constructor - creates an invalid message
     */
    Message();

    /**
     * @brief Full constructor with all parameters
     */
    Message(const std::string& imei,
            uint32_t sequence,
            MessageType messageType,
            ProtocolVersion version,
            const FieldMap& fields,
            const Timestamp& timestamp);

    /**
     * @brief Constructor with default timestamp
     */
    Message(const std::string& imei,
            uint32_t sequence,
            MessageType messageType,
            ProtocolVersion version);

    /**
     * @brief Constructor for tests (MessageType, IMEI, sequence)
     */
    Message(MessageType messageType, const std::string& imei, uint32_t sequence);

    /**
     * @brief Copy constructor
     */
    Message(const Message& other) = default;

    /**
     * @brief Move constructor
     */
    Message(Message&& other) noexcept = default;

    /**
     * @brief Copy assignment
     */
    Message& operator=(const Message& other) = default;

    /**
     * @brief Move assignment
     */
    Message& operator=(Message&& other) noexcept = default;

    /**
     * @brief Destructor
     */
    ~Message() = default;

    // Basic accessors
    std::string getImei() const { return imei_; }
    uint32_t getSequence() const { return sequence_; }
    MessageType getType() const { return messageType_; }
    ProtocolVersion getProtocolVersion() const { return version_; }
    Timestamp getTimestamp() const { return timestamp_; }

    // Validation
    bool isValid() const { return validationErrors_.empty(); }
    const std::vector<ValidationErrorInfo>& getValidationErrors() const { return validationErrors_; }
    void addValidationError(const ValidationErrorInfo& error) { validationErrors_.push_back(error); }

    // Protocol version management
    void setProtocolVersion(const ProtocolVersion& version) { version_ = version; }

    // Field management
    template<typename T>
    bool addField(const std::string& name, const T& value) {
        if (fields_.find(name) != fields_.end()) {
            return false;  // Field already exists
        }
        fields_[name] = Value(value);
        return true;
    }

    template<typename T>
    void setField(const std::string& name, const T& value) {
        fields_[name] = Value(value);
    }

    void removeField(const std::string& name) {
        fields_.erase(name);
    }

    bool hasField(const std::string& name) const {
        return fields_.find(name) != fields_.end();
    }

    // Field access with type conversion
    template<typename T>
    T getField(const std::string& name) const {
        auto it = fields_.find(name);
        if (it == fields_.end()) {
            throw FieldAccessError("Field '" + name + "' not found in message");
        }
        return convertValue<T>(it->second);
    }

    template<typename T>
    std::optional<T> tryGetField(const std::string& name) const {
        auto it = fields_.find(name);
        if (it == fields_.end()) {
            return std::nullopt;
        }
        try {
            return convertValue<T>(it->second);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // Get all field names
    std::vector<std::string> getFieldNames() const;

    // Serialization
    nlohmann::json toJson() const;
    std::vector<uint8_t> toBinary() const;
    static Message fromBinary(const std::vector<uint8_t>& data);

    // Field map access
    const FieldMap& getFields() const { return fields_; }

private:
    std::string imei_;
    uint32_t sequence_;
    MessageType messageType_;
    ProtocolVersion version_;
    Timestamp timestamp_;
    FieldMap fields_;
    std::vector<ValidationErrorInfo> validationErrors_;

    // Type conversion helper
    template<typename T>
    T convertValue(const Value& value) const {
        // Handle exact type matches first
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }

        // Handle numeric conversions
        if constexpr (std::is_arithmetic_v<T>) {
            return std::visit([](const auto& v) -> T {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_arithmetic_v<V> && !std::is_same_v<V, std::monostate>) {
                    return static_cast<T>(v);
                } else {
                    throw TypeConversionError("Cannot convert to requested numeric type");
                }
            }, value);
        }

        // Handle string conversions
        if constexpr (std::is_same_v<T, std::string>) {
            if (std::holds_alternative<std::string>(value)) {
                return std::get<std::string>(value);
            }
        }

        throw TypeConversionError("Cannot convert field value to requested type");
    }

    friend class Parser;
    friend class CommandBuilder;
};

} // namespace protocol_parser