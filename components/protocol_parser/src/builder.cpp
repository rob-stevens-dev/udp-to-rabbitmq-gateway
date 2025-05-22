#include "protocol_parser/builder.hpp"
#include "protocol_parser/error.hpp"
#include <sstream>

namespace protocol_parser {

CommandBuilder::CommandBuilder(CommandType type) 
    : commandType_(type)
    , sequence_(0)
{
}

CommandBuilder CommandBuilder::create(CommandType type) {
    return CommandBuilder(type);
}

CommandBuilder& CommandBuilder::setImei(const std::string& imei) {
    if (imei.length() != 15) {
        throw ValidationError("IMEI must be exactly 15 digits");
    }
    imei_ = imei;
    return *this;
}

CommandBuilder& CommandBuilder::setSequence(uint32_t sequence) {
    sequence_ = sequence;
    return *this;
}

CommandBuilder& CommandBuilder::addParameter(const std::string& name, const std::string& value) {
    parameters_[name] = ParameterValue(value);
    return *this;
}

Message CommandBuilder::build() {
    if (imei_.empty()) {
        throw ValidationError("IMEI must be set before building command");
    }

    // Convert CommandType to MessageType
    MessageType messageType;
    switch (commandType_) {
        case CommandType::REBOOT:
            messageType = MessageType::COMMAND_REBOOT;
            break;
        case CommandType::SUBSCRIBE:
            messageType = MessageType::COMMAND_SUBSCRIBE;
            break;
        case CommandType::UNSUBSCRIBE:
            messageType = MessageType::COMMAND_UNSUBSCRIBE;
            break;
        case CommandType::UPDATE_CONFIG:
            messageType = MessageType::COMMAND_UPDATE_CONFIG;
            break;
        default:
            messageType = MessageType::COMMAND_REBOOT;
            break;
    }

    // Create the message
    Message message(messageType, imei_, sequence_);

    // Add command type field
    message.setField("command_type", static_cast<uint32_t>(commandType_));

    // Add all parameters as fields
    for (const auto& [name, value] : parameters_) {
        std::visit([&](const auto& v) {
            message.setField(name, v);
        }, value);
    }

    return message;
}

std::vector<uint8_t> CommandBuilder::toBinary() {
    Message message = build();
    return message.toBinary();
}

std::string CommandBuilder::parameterValueToString(const ParameterValue& value) const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else {
            return std::to_string(v);
        }
    }, value);
}

// Command class implementation
Command::Command(const Message& message) : message_(message) {
}

std::vector<uint8_t> Command::toBinary() const {
    return message_.toBinary();
}

CommandType Command::getType() const {
    if (message_.hasField("command_type")) {
        try {
            uint32_t commandTypeValue = message_.getField<uint32_t>("command_type");
            return static_cast<CommandType>(commandTypeValue);
        } catch (const std::exception&) {
            // Fall back to determining from message type
        }
    }

    // Determine command type from message type
    switch (message_.getType()) {
        case MessageType::COMMAND_REBOOT:
            return CommandType::REBOOT;
        case MessageType::COMMAND_SUBSCRIBE:
            return CommandType::SUBSCRIBE;
        case MessageType::COMMAND_UNSUBSCRIBE:
            return CommandType::UNSUBSCRIBE;
        case MessageType::COMMAND_UPDATE_CONFIG:
            return CommandType::UPDATE_CONFIG;
        default:
            return CommandType::REBOOT;
    }
}

std::string Command::getImei() const {
    return message_.getImei();
}

uint32_t Command::getSequence() const {
    return message_.getSequence();
}

} // namespace protocol_parser