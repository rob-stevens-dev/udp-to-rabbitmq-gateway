#include "protocol_parser/parser.hpp"
#include "protocol_parser/error.hpp"
#include <algorithm>
#include <cstring>

namespace protocol_parser {

Parser::Parser(const Config& config) : config_(config), lastError_("No error") {
    // Initialize parser with configuration
}

Message Parser::parse(const std::vector<uint8_t>& packet) {
    if (packet.empty()) {
        throw ParseError("Empty packet data");
    }
    return parseImpl(packet.data(), packet.size());
}

Message Parser::parse(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        throw ParseError("Invalid data pointer or zero length");
    }
    return parseImpl(data, length);
}

Message Parser::parseImpl(const uint8_t* data, size_t length) {
    try {
        // Reset last error
        lastError_ = ParseError("No error");

        // Minimum packet size check
        if (length < MINIMUM_PACKET_SIZE) {
            throw ParseError("Packet too small: " + std::to_string(length) + " bytes");
        }

        // Parse header
        PacketHeader header;
        if (!parseHeader(data, length, header)) {
            throw ParseError("Failed to parse packet header");
        }

        // Create message
        Message message(header.imei, header.sequence, header.type, header.version);

        // Validate header data
        std::vector<ValidationErrorInfo> errors;
        validateHeader(header, errors);
        
        // Add validation errors to message
        for (const auto& error : errors) {
            message.addValidationError(error);
        }

        // Parse payload based on message type
        size_t payloadOffset = HEADER_SIZE;
        size_t payloadSize = length - payloadOffset - CHECKSUM_SIZE;
        
        if (payloadSize > 0) {
            parsePayload(data + payloadOffset, payloadSize, header.type, message);
        }

        // Verify checksum
        if (!verifyChecksum(data, length)) {
            message.addValidationError(ValidationErrorInfo{
                "Checksum verification failed",
                "checksum",
                ErrorSeverity::ERROR,
                ErrorCategory::PROTOCOL_ERROR
            });
        }

        return message;
    }
    catch (const std::exception& e) {
        lastError_ = ParseError(e.what());
        throw;
    }
}

bool Parser::parseHeader(const uint8_t* data, size_t length, PacketHeader& header) {
    if (length < HEADER_SIZE) {
        return false;
    }

    size_t offset = 0;

    // Magic bytes
    if (data[offset] != 0xAA || data[offset + 1] != 0xBB) {
        return false;
    }
    offset += 2;

    // Protocol version
    header.version.major = data[offset++];
    header.version.minor = data[offset++];

    // Message type
    header.type = static_cast<MessageType>(data[offset++]);

    // IMEI (15 bytes BCD encoded)
    header.imei.clear();
    for (int i = 0; i < 15; ++i) {
        uint8_t digit = data[offset + i / 2];
        if (i % 2 == 0) {
            digit = digit & 0x0F;
        } else {
            digit = (digit >> 4) & 0x0F;
        }
        if (digit <= 9) {
            header.imei += ('0' + digit);
        }
    }
    offset += 8; // 15 digits in 8 bytes (BCD)

    // Sequence number (4 bytes, little endian)
    header.sequence = data[offset] | (data[offset + 1] << 8) | 
                     (data[offset + 2] << 16) | (data[offset + 3] << 24);
    offset += 4;

    return true;
}

void Parser::validateHeader(const PacketHeader& header, std::vector<ValidationErrorInfo>& errors) {
    // Validate protocol version
    if (header.version.major > 2 || (header.version.major == 2 && header.version.minor > 0)) {
        errors.push_back(ValidationErrorInfo{
            "Unsupported protocol version: " + std::to_string(header.version.major) + 
            "." + std::to_string(header.version.minor),
            "version",
            ErrorSeverity::WARNING,
            ErrorCategory::PROTOCOL_ERROR
        });
    }

    // Validate message type
    if (header.type == MessageType::UNKNOWN || static_cast<int>(header.type) > 10) {
        errors.push_back(ValidationErrorInfo{
            "Unknown message type: " + std::to_string(static_cast<int>(header.type)),
            "messageType",
            ErrorSeverity::ERROR,
            ErrorCategory::PROTOCOL_ERROR
        });
    }

    // Validate IMEI
    if (header.imei.length() != 15) {
        errors.push_back(ValidationErrorInfo{
            "Invalid IMEI length: " + std::to_string(header.imei.length()),
            "imei",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        });
    }

    // Check if IMEI contains only digits
    if (!std::all_of(header.imei.begin(), header.imei.end(), ::isdigit)) {
        errors.push_back(ValidationErrorInfo{
            "IMEI contains non-digit characters",
            "imei",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        });
    }
}

void Parser::parsePayload(const uint8_t* data, size_t length, MessageType type, Message& message) {
    switch (type) {
        case MessageType::GPS_UPDATE:
            parseGPSUpdate(data, length, message);
            break;
        case MessageType::STATUS_UPDATE:
            parseStatusUpdate(data, length, message);
            break;
        case MessageType::EVENT_NOTIFICATION:
            parseEventNotification(data, length, message);
            break;
        case MessageType::COMMAND_ACK:
            parseCommandAck(data, length, message);
            break;
        default:
            // Store raw payload for unknown message types
            std::vector<uint8_t> rawPayload(data, data + length);
            message.setField("raw_payload", rawPayload);
            break;
    }
}

void Parser::parseGPSUpdate(const uint8_t* data, size_t length, Message& message) {
    if (length < 32) { // Minimum GPS payload size
        message.addValidationError(ValidationErrorInfo{
            "GPS payload too small",
            "gps_payload",
            ErrorSeverity::ERROR
        });
        return;
    }

    size_t offset = 0;

    // Latitude (4 bytes, signed, scaled by 1e6)
    int32_t latRaw = data[offset] | (data[offset + 1] << 8) | 
                     (data[offset + 2] << 16) | (data[offset + 3] << 24);
    double latitude = static_cast<double>(latRaw) / 1000000.0;
    message.setField("latitude", latitude);
    offset += 4;

    // Longitude (4 bytes, signed, scaled by 1e6)
    int32_t lonRaw = data[offset] | (data[offset + 1] << 8) | 
                     (data[offset + 2] << 16) | (data[offset + 3] << 24);
    double longitude = static_cast<double>(lonRaw) / 1000000.0;
    message.setField("longitude", longitude);
    offset += 4;

    // Altitude (2 bytes, signed, meters)
    int16_t altitude = data[offset] | (data[offset + 1] << 8);
    message.setField("altitude", static_cast<int32_t>(altitude));
    offset += 2;

    // Speed (2 bytes, unsigned, km/h * 10)
    uint16_t speedRaw = data[offset] | (data[offset + 1] << 8);
    float speed = static_cast<float>(speedRaw) / 10.0f;
    message.setField("speed", speed);
    offset += 2;

    // Heading (2 bytes, unsigned, degrees * 10)
    uint16_t headingRaw = data[offset] | (data[offset + 1] << 8);
    float heading = static_cast<float>(headingRaw) / 10.0f;
    message.setField("heading", heading);
    offset += 2;

    // Number of satellites (1 byte)
    uint8_t satellites = data[offset++];
    message.setField("satellites", static_cast<uint32_t>(satellites));

    // HDOP (2 bytes, unsigned, * 100)
    uint16_t hdopRaw = data[offset] | (data[offset + 1] << 8);
    float hdop = static_cast<float>(hdopRaw) / 100.0f;
    message.setField("hdop", hdop);
    offset += 2;

    // Fix type (1 byte)
    uint8_t fixType = data[offset++];
    message.setField("fix_type", static_cast<uint32_t>(fixType));

    // GPS timestamp (4 bytes, Unix timestamp)
    if (offset + 4 <= length) {
        uint32_t timestamp = data[offset] | (data[offset + 1] << 8) | 
                            (data[offset + 2] << 16) | (data[offset + 3] << 24);
        message.setField("gps_timestamp", timestamp);
        offset += 4;
    }
}

void Parser::parseStatusUpdate(const uint8_t* data, size_t length, Message& message) {
    if (length < 8) {
        message.addValidationError(ValidationErrorInfo{
            "Status payload too small",
            "status_payload",
            ErrorSeverity::ERROR
        });
        return;
    }

    size_t offset = 0;

    // Battery level (1 byte, percentage)
    uint8_t batteryLevel = data[offset++];
    message.setField("battery_level", static_cast<uint32_t>(batteryLevel));

    // Signal strength (1 byte, signed dBm)
    int8_t signalStrength = static_cast<int8_t>(data[offset++]);
    message.setField("signal_strength", static_cast<int32_t>(signalStrength));

    // Temperature (2 bytes, signed, Celsius * 10)
    int16_t tempRaw = data[offset] | (data[offset + 1] << 8);
    float temperature = static_cast<float>(tempRaw) / 10.0f;
    message.setField("temperature", temperature);
    offset += 2;

    // Voltage (2 bytes, unsigned, mV)
    uint16_t voltage = data[offset] | (data[offset + 1] << 8);
    message.setField("voltage", static_cast<uint32_t>(voltage));
    offset += 2;

    // Status flags (1 byte)
    uint8_t statusFlags = data[offset++];
    message.setField("status_flags", static_cast<uint32_t>(statusFlags));

    // Parse individual status flags
    message.setField("is_charging", static_cast<bool>(statusFlags & 0x01));
    message.setField("is_moving", static_cast<bool>(statusFlags & 0x02));
    message.setField("gps_enabled", static_cast<bool>(statusFlags & 0x04));
    message.setField("panic_button", static_cast<bool>(statusFlags & 0x08));
}

void Parser::parseEventNotification(const uint8_t* data, size_t length, Message& message) {
    if (length < 2) {
        message.addValidationError(ValidationErrorInfo{
            "Event payload too small",
            "event_payload",
            ErrorSeverity::ERROR
        });
        return;
    }

    size_t offset = 0;

    // Event type (1 byte)
    uint8_t eventType = data[offset++];
    message.setField("event_type", static_cast<uint32_t>(eventType));

    // Event data length (1 byte)
    uint8_t eventDataLength = data[offset++];

    // Event data (variable length)
    if (offset + eventDataLength <= length) {
        std::vector<uint8_t> eventData(data + offset, data + offset + eventDataLength);
        message.setField("event_data", eventData);
        offset += eventDataLength;
    }

    // Event timestamp (4 bytes, Unix timestamp)
    if (offset + 4 <= length) {
        uint32_t eventTimestamp = data[offset] | (data[offset + 1] << 8) | 
                                 (data[offset + 2] << 16) | (data[offset + 3] << 24);
        message.setField("event_timestamp", eventTimestamp);
        offset += 4;
    }
}

void Parser::parseCommandAck(const uint8_t* data, size_t length, Message& message) {
    if (length < 6) {
        message.addValidationError(ValidationErrorInfo{
            "Command ACK payload too small",
            "ack_payload",
            ErrorSeverity::ERROR
        });
        return;
    }

    size_t offset = 0;

    // Original command sequence (4 bytes)
    uint32_t originalSequence = data[offset] | (data[offset + 1] << 8) | 
                               (data[offset + 2] << 16) | (data[offset + 3] << 24);
    message.setField("original_sequence", originalSequence);
    offset += 4;

    // Result code (1 byte)
    uint8_t resultCode = data[offset++];
    message.setField("result_code", static_cast<uint32_t>(resultCode));

    // Error message length (1 byte)
    uint8_t errorMessageLength = data[offset++];

    // Error message (variable length)
    if (offset + errorMessageLength <= length && errorMessageLength > 0) {
        std::string errorMessage(reinterpret_cast<const char*>(data + offset), errorMessageLength);
        message.setField("error_message", errorMessage);
        offset += errorMessageLength;
    }
}

bool Parser::verifyChecksum(const uint8_t* data, size_t length) {
    if (length < CHECKSUM_SIZE) {
        return false;
    }

    // Calculate XOR checksum of all bytes except the last one (which is the checksum)
    uint8_t calculatedChecksum = 0;
    for (size_t i = 0; i < length - 1; ++i) {
        calculatedChecksum ^= data[i];
    }

    uint8_t packetChecksum = data[length - 1];
    return calculatedChecksum == packetChecksum;
}

} // namespace protocol_parser