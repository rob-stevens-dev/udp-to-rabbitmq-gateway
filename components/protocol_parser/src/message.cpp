#include "protocol_parser/message.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace protocol_parser {

Message::Message() 
    : imei_("")
    , sequence_(0)
    , messageType_(MessageType::UNKNOWN)
    , version_{1, 0}
    , timestamp_(std::chrono::system_clock::now())
{
    // Default constructor creates an invalid message
    validationErrors_.push_back(ValidationErrorInfo{
        "Default constructed message is invalid",
        "message",
        ErrorSeverity::ERROR
    });
}

Message::Message(const std::string& imei,
                 uint32_t sequence,
                 MessageType messageType,
                 ProtocolVersion version,
                 const FieldMap& fields,
                 const Timestamp& timestamp)
    : imei_(imei)
    , sequence_(sequence)
    , messageType_(messageType)
    , version_(version)
    , timestamp_(timestamp)
    , fields_(fields)
{
    // Validate IMEI format (should be 15 digits)
    if (imei_.length() != 15 || !std::all_of(imei_.begin(), imei_.end(), ::isdigit)) {
        validationErrors_.push_back(ValidationErrorInfo{
            "IMEI must be exactly 15 digits",
            "imei",
            ErrorSeverity::ERROR
        });
    }
}

Message::Message(const std::string& imei,
                 uint32_t sequence,
                 MessageType messageType,
                 ProtocolVersion version)
    : Message(imei, sequence, messageType, version, {}, std::chrono::system_clock::now())
{
}

Message::Message(MessageType messageType, const std::string& imei, uint32_t sequence)
    : Message(imei, sequence, messageType, ProtocolVersion{1, 0}, {}, std::chrono::system_clock::now())
{
}

std::vector<std::string> Message::getFieldNames() const {
    std::vector<std::string> names;
    names.reserve(fields_.size());
    for (const auto& [name, value] : fields_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

nlohmann::json Message::toJson() const {
    nlohmann::json json;
    
    // Basic message info
    json["imei"] = imei_;
    json["sequence"] = sequence_;
    json["messageType"] = static_cast<int>(messageType_);
    json["version"] = {
        {"major", version_.major},
        {"minor", version_.minor}
    };
    
    // Timestamp as ISO string
    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ss.str();
    
    // Fields
    nlohmann::json fieldsJson;
    for (const auto& [name, value] : fields_) {
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                fieldsJson[name] = nullptr;
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                // Convert binary data to hex string
                std::stringstream hexStream;
                for (uint8_t byte : v) {
                    hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                }
                fieldsJson[name] = hexStream.str();
            } else {
                fieldsJson[name] = v;
            }
        }, value);
    }
    json["fields"] = fieldsJson;
    
    // Validation errors
    if (!validationErrors_.empty()) {
        nlohmann::json errorsJson = nlohmann::json::array();
        for (const auto& error : validationErrors_) {
            errorsJson.push_back({
                {"message", error.message},
                {"field", error.field},
                {"severity", static_cast<int>(error.severity)}
            });
        }
        json["validationErrors"] = errorsJson;
    }
    
    return json;
}

std::vector<uint8_t> Message::toBinary() const {
    std::vector<uint8_t> binary;
    
    // This is a simplified binary format for testing
    // In a real implementation, this would follow the actual protocol specification
    
    // Header (magic bytes)
    binary.push_back(0xAA);
    binary.push_back(0xBB);
    
    // Message type
    binary.push_back(static_cast<uint8_t>(messageType_));
    
    // Version
    binary.push_back(version_.major);
    binary.push_back(version_.minor);
    
    // IMEI (15 bytes, padded with zeros if needed)
    for (size_t i = 0; i < 15; ++i) {
        if (i < imei_.length()) {
            binary.push_back(static_cast<uint8_t>(imei_[i] - '0'));
        } else {
            binary.push_back(0);
        }
    }
    
    // Sequence number (4 bytes, little endian)
    binary.push_back(sequence_ & 0xFF);
    binary.push_back((sequence_ >> 8) & 0xFF);
    binary.push_back((sequence_ >> 16) & 0xFF);
    binary.push_back((sequence_ >> 24) & 0xFF);
    
    // Field count
    binary.push_back(static_cast<uint8_t>(fields_.size()));
    
    // Fields (simplified encoding)
    for (const auto& [name, value] : fields_) {
        // Field name length and name
        binary.push_back(static_cast<uint8_t>(name.length()));
        for (char c : name) {
            binary.push_back(static_cast<uint8_t>(c));
        }
        
        // Field value (type and data)
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                binary.push_back(0); // null type
            } else if constexpr (std::is_same_v<T, bool>) {
                binary.push_back(1);
                binary.push_back(v ? 1 : 0);
            } else if constexpr (std::is_same_v<T, int8_t>) {
                binary.push_back(2);
                binary.push_back(static_cast<uint8_t>(v));
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                binary.push_back(3);
                binary.push_back(v);
            } else if constexpr (std::is_same_v<T, int16_t>) {
                binary.push_back(4);
                binary.push_back(v & 0xFF);
                binary.push_back((v >> 8) & 0xFF);
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                binary.push_back(5);
                binary.push_back(v & 0xFF);
                binary.push_back((v >> 8) & 0xFF);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                binary.push_back(6);
                binary.push_back(v & 0xFF);
                binary.push_back((v >> 8) & 0xFF);
                binary.push_back((v >> 16) & 0xFF);
                binary.push_back((v >> 24) & 0xFF);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                binary.push_back(7);
                binary.push_back(v & 0xFF);
                binary.push_back((v >> 8) & 0xFF);
                binary.push_back((v >> 16) & 0xFF);
                binary.push_back((v >> 24) & 0xFF);
            } else if constexpr (std::is_same_v<T, float>) {
                binary.push_back(10);
                uint32_t floatBits = *reinterpret_cast<const uint32_t*>(&v);
                binary.push_back(floatBits & 0xFF);
                binary.push_back((floatBits >> 8) & 0xFF);
                binary.push_back((floatBits >> 16) & 0xFF);
                binary.push_back((floatBits >> 24) & 0xFF);
            } else if constexpr (std::is_same_v<T, double>) {
                binary.push_back(11);
                uint64_t doubleBits = *reinterpret_cast<const uint64_t*>(&v);
                for (int i = 0; i < 8; ++i) {
                    binary.push_back((doubleBits >> (i * 8)) & 0xFF);
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                binary.push_back(12);
                binary.push_back(static_cast<uint8_t>(v.length()));
                for (char c : v) {
                    binary.push_back(static_cast<uint8_t>(c));
                }
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                binary.push_back(13);
                binary.push_back(static_cast<uint8_t>(v.size()));
                for (uint8_t byte : v) {
                    binary.push_back(byte);
                }
            }
        }, value);
    }
    
    // Simple checksum (XOR of all bytes)
    uint8_t checksum = 0;
    for (uint8_t byte : binary) {
        checksum ^= byte;
    }
    binary.push_back(checksum);
    
    return binary;
}

Message Message::fromBinary(const std::vector<uint8_t>& data) {
    if (data.size() < 25) { // Minimum size for header
        throw ParseError("Binary data too short for valid message");
    }
    
    size_t offset = 0;
    
    // Check magic bytes
    if (data[offset] != 0xAA || data[offset + 1] != 0xBB) {
        throw ParseError("Invalid magic bytes in binary data");
    }
    offset += 2;
    
    // Read message type
    MessageType messageType = static_cast<MessageType>(data[offset++]);
    
    // Read version
    ProtocolVersion version{data[offset], data[offset + 1]};
    offset += 2;
    
    // Read IMEI
    std::string imei;
    for (int i = 0; i < 15; ++i) {
        imei += std::to_string(data[offset + i]);
    }
    offset += 15;
    
    // Read sequence number
    uint32_t sequence = data[offset] | (data[offset + 1] << 8) | 
                       (data[offset + 2] << 16) | (data[offset + 3] << 24);
    offset += 4;
    
    // Create message
    Message message(imei, sequence, messageType, version);
    
    // Read field count
    uint8_t fieldCount = data[offset++];
    
    // Read fields
    for (int i = 0; i < fieldCount && offset < data.size() - 1; ++i) {
        // Read field name
        uint8_t nameLength = data[offset++];
        if (offset + nameLength >= data.size()) break;
        
        std::string fieldName;
        for (int j = 0; j < nameLength; ++j) {
            fieldName += static_cast<char>(data[offset++]);
        }
        
        // Read field type and value
        if (offset >= data.size()) break;
        uint8_t fieldType = data[offset++];
        
        switch (fieldType) {
            case 1: // bool
                if (offset < data.size()) {
                    message.addField(fieldName, data[offset++] != 0);
                }
                break;
            case 2: // int8_t
                if (offset < data.size()) {
                    message.addField(fieldName, static_cast<int8_t>(data[offset++]));
                }
                break;
            case 3: // uint8_t
                if (offset < data.size()) {
                    message.addField(fieldName, data[offset++]);
                }
                break;
            case 10: // float
                if (offset + 4 <= data.size()) {
                    uint32_t floatBits = data[offset] | (data[offset + 1] << 8) |
                                       (data[offset + 2] << 16) | (data[offset + 3] << 24);
                    float value = *reinterpret_cast<float*>(&floatBits);
                    message.addField(fieldName, value);
                    offset += 4;
                }
                break;
            case 12: // string
                if (offset < data.size()) {
                    uint8_t strLength = data[offset++];
                    if (offset + strLength <= data.size()) {
                        std::string value;
                        for (int j = 0; j < strLength; ++j) {
                            value += static_cast<char>(data[offset++]);
                        }
                        message.addField(fieldName, value);
                    }
                }
                break;
            default:
                // Skip unknown field types
                break;
        }
    }
    
    return message;
}

} // namespace protocol_parser