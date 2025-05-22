#pragma once

#include "protocol_parser/types.hpp"
#include "protocol_parser/message.hpp"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace protocol_parser {

/**
 * @brief Command builder for creating protocol command messages
 * 
 * This class provides a fluent interface for building command messages
 * that can be sent to devices via the protocol.
 */
class CommandBuilder {
public:
    // Parameter value type
    using ParameterValue = std::variant<
        bool,
        int32_t,
        uint32_t,
        float,
        double,
        std::string
    >;

    /**
     * @brief Create a command builder for the specified command type
     * @param type Command type to build
     * @return New CommandBuilder instance
     */
    static CommandBuilder create(CommandType type);

    /**
     * @brief Set the target device IMEI
     * @param imei Device IMEI (15 digits)
     * @return Reference to this builder for chaining
     */
    CommandBuilder& setImei(const std::string& imei);

    /**
     * @brief Set the command sequence number
     * @param sequence Sequence number for the command
     * @return Reference to this builder for chaining
     */
    CommandBuilder& setSequence(uint32_t sequence);

    /**
     * @brief Add a parameter to the command (string version)
     * @param name Parameter name
     * @param value Parameter value as string
     * @return Reference to this builder for chaining
     */
    CommandBuilder& addParameter(const std::string& name, const std::string& value);

    /**
     * @brief Add a parameter to the command (template version for any type)
     * @param name Parameter name
     * @param value Parameter value of any supported type
     * @return Reference to this builder for chaining
     */
    template<typename T>
    CommandBuilder& addParameter(const std::string& name, T value) {
        parameters_[name] = ParameterValue(value);
        return *this;
    }

    /**
     * @brief Build the final command message
     * @return Built Message representing the command
     */
    Message build();

    /**
     * @brief Get the binary representation of the command
     * @return Binary command data
     */
    std::vector<uint8_t> toBinary();

    /**
     * @brief Get the command type
     * @return The command type being built
     */
    CommandType getType() const { return commandType_; }

private:
    CommandType commandType_;
    std::string imei_;
    uint32_t sequence_;
    std::unordered_map<std::string, ParameterValue> parameters_;

    /**
     * @brief Private constructor - use create() method instead
     */
    explicit CommandBuilder(CommandType type);

    /**
     * @brief Convert parameter value to string for serialization
     */
    std::string parameterValueToString(const ParameterValue& value) const;
};

/**
 * @brief Represents a built command
 */
class Command {
public:
    /**
     * @brief Constructor from Message
     */
    explicit Command(const Message& message);

    /**
     * @brief Get the underlying message
     */
    const Message& getMessage() const { return message_; }

    /**
     * @brief Convert to binary representation
     */
    std::vector<uint8_t> toBinary() const;

    /**
     * @brief Get command type
     */
    CommandType getType() const;

    /**
     * @brief Get target IMEI
     */
    std::string getImei() const;

    /**
     * @brief Get sequence number
     */
    uint32_t getSequence() const;

private:
    Message message_;
};

} // namespace protocol_parser