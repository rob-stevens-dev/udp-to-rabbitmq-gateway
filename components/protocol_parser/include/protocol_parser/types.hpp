#pragma once

#include <cstdint>
#include <string>

namespace protocol_parser {

/**
 * @brief Protocol version structure
 */
struct ProtocolVersion {
    uint8_t major;
    uint8_t minor;
    
    ProtocolVersion() : major(1), minor(0) {}
    ProtocolVersion(uint8_t maj, uint8_t min) : major(maj), minor(min) {}
    
    bool operator==(const ProtocolVersion& other) const {
        return major == other.major && minor == other.minor;
    }
    
    bool operator!=(const ProtocolVersion& other) const {
        return !(*this == other);
    }
    
    bool operator<(const ProtocolVersion& other) const {
        if (major != other.major) return major < other.major;
        return minor < other.minor;
    }
};

/**
 * @brief Message types supported by the protocol
 */
enum class MessageType : uint8_t {
    UNKNOWN = 0,
    GPS_UPDATE = 1,
    STATUS_UPDATE = 2,
    EVENT_NOTIFICATION = 3,
    COMMAND_ACK = 4,
    COMMAND_REBOOT = 5,
    COMMAND_SUBSCRIBE = 6,
    COMMAND_UNSUBSCRIBE = 7,
    COMMAND_UPDATE_CONFIG = 8,
    HEARTBEAT = 9,
    DEBUG_INFO = 10
};

/**
 * @brief Command types that can be sent to devices
 */
enum class CommandType : uint8_t {
    REBOOT = 1,
    SUBSCRIBE = 2,
    UNSUBSCRIBE = 3,
    UPDATE_CONFIG = 4,
    GET_STATUS = 5,
    SET_INTERVAL = 6,
    FACTORY_RESET = 7
};

/**
 * @brief Field data types supported by the protocol
 */
enum class FieldType : uint8_t {
    UNKNOWN = 0,
    BOOLEAN = 1,
    INT8 = 2,
    UINT8 = 3,
    INT16 = 4,
    UINT16 = 5,
    INT32 = 6,
    UINT32 = 7,
    INT64 = 8,
    UINT64 = 9,
    FLOAT = 10,
    DOUBLE = 11,
    STRING = 12,
    BINARY = 13
};

/**
 * @brief GPS fix types
 */
enum class GPSFixType : uint8_t {
    NO_FIX = 0,
    GPS_FIX_2D = 1,
    GPS_FIX_3D = 2,
    DGPS_FIX = 3
};

/**
 * @brief Event types for notifications
 */
enum class EventType : uint8_t {
    UNKNOWN = 0,
    PANIC_BUTTON = 1,
    GEOFENCE_ENTER = 2,
    GEOFENCE_EXIT = 3,
    SPEED_LIMIT_EXCEEDED = 4,
    LOW_BATTERY = 5,
    POWER_ON = 6,
    POWER_OFF = 7,
    EXTERNAL_POWER_CONNECTED = 8,
    EXTERNAL_POWER_DISCONNECTED = 9,
    TEMPERATURE_ALERT = 10
};

/**
 * @brief Device status flags
 */
enum class StatusFlag : uint8_t {
    CHARGING = 0x01,
    MOVING = 0x02,
    GPS_ENABLED = 0x04,
    PANIC_BUTTON_PRESSED = 0x08,
    EXTERNAL_POWER = 0x10,
    LOW_BATTERY = 0x20,
    TEMPERATURE_ALERT = 0x40,
    DEBUG_MODE = 0x80
};

/**
 * @brief Protocol constants
 */
namespace Constants {
    constexpr size_t IMEI_LENGTH = 15;
    constexpr size_t MAX_PAYLOAD_SIZE = 4096;
    constexpr size_t HEADER_SIZE = 19;
    constexpr size_t CHECKSUM_SIZE = 1;
    constexpr size_t MIN_PACKET_SIZE = HEADER_SIZE + CHECKSUM_SIZE;
    
    constexpr uint8_t MAGIC_BYTE_1 = 0xAA;
    constexpr uint8_t MAGIC_BYTE_2 = 0xBB;
    
    constexpr double MAX_LATITUDE = 90.0;
    constexpr double MIN_LATITUDE = -90.0;
    constexpr double MAX_LONGITUDE = 180.0;
    constexpr double MIN_LONGITUDE = -180.0;
    constexpr float MAX_SPEED_KMH = 500.0f;
    constexpr float MAX_HDOP = 99.9f;
    constexpr int8_t MIN_SIGNAL_STRENGTH = -120;
    constexpr int8_t MAX_SIGNAL_STRENGTH = 0;
}

/**
 * @brief Convert MessageType to string representation
 * @param type MessageType to convert
 * @return String representation of the message type
 */
inline const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::UNKNOWN: return "UNKNOWN";
        case MessageType::GPS_UPDATE: return "GPS_UPDATE";
        case MessageType::STATUS_UPDATE: return "STATUS_UPDATE";
        case MessageType::EVENT_NOTIFICATION: return "EVENT_NOTIFICATION";
        case MessageType::COMMAND_ACK: return "COMMAND_ACK";
        case MessageType::COMMAND_REBOOT: return "COMMAND_REBOOT";
        case MessageType::COMMAND_SUBSCRIBE: return "COMMAND_SUBSCRIBE";
        case MessageType::COMMAND_UNSUBSCRIBE: return "COMMAND_UNSUBSCRIBE";
        case MessageType::COMMAND_UPDATE_CONFIG: return "COMMAND_UPDATE_CONFIG";
        case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::DEBUG_INFO: return "DEBUG_INFO";
        default: return "INVALID";
    }
}

/**
 * @brief Convert CommandType to string representation
 * @param type CommandType to convert
 * @return String representation of the command type
 */
inline const char* commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::REBOOT: return "REBOOT";
        case CommandType::SUBSCRIBE: return "SUBSCRIBE";
        case CommandType::UNSUBSCRIBE: return "UNSUBSCRIBE";
        case CommandType::UPDATE_CONFIG: return "UPDATE_CONFIG";
        case CommandType::GET_STATUS: return "GET_STATUS";
        case CommandType::SET_INTERVAL: return "SET_INTERVAL";
        case CommandType::FACTORY_RESET: return "FACTORY_RESET";
        default: return "INVALID";
    }
}

/**
 * @brief Convert FieldType to string representation
 * @param type FieldType to convert
 * @return String representation of the field type
 */
inline const char* fieldTypeToString(FieldType type) {
    switch (type) {
        case FieldType::UNKNOWN: return "UNKNOWN";
        case FieldType::BOOLEAN: return "BOOLEAN";
        case FieldType::INT8: return "INT8";
        case FieldType::UINT8: return "UINT8";
        case FieldType::INT16: return "INT16";
        case FieldType::UINT16: return "UINT16";
        case FieldType::INT32: return "INT32";
        case FieldType::UINT32: return "UINT32";
        case FieldType::INT64: return "INT64";
        case FieldType::UINT64: return "UINT64";
        case FieldType::FLOAT: return "FLOAT";
        case FieldType::DOUBLE: return "DOUBLE";
        case FieldType::STRING: return "STRING";
        case FieldType::BINARY: return "BINARY";
        default: return "INVALID";
    }
}

/**
 * @brief Convert GPSFixType to string representation
 * @param type GPSFixType to convert
 * @return String representation of the GPS fix type
 */
inline const char* gpsFixTypeToString(GPSFixType type) {
    switch (type) {
        case GPSFixType::NO_FIX: return "NO_FIX";
        case GPSFixType::GPS_FIX_2D: return "GPS_FIX_2D";
        case GPSFixType::GPS_FIX_3D: return "GPS_FIX_3D";
        case GPSFixType::DGPS_FIX: return "DGPS_FIX";
        default: return "INVALID";
    }
}

/**
 * @brief Convert EventType to string representation
 * @param type EventType to convert
 * @return String representation of the event type
 */
inline const char* eventTypeToString(EventType type) {
    switch (type) {
        case EventType::UNKNOWN: return "UNKNOWN";
        case EventType::PANIC_BUTTON: return "PANIC_BUTTON";
        case EventType::GEOFENCE_ENTER: return "GEOFENCE_ENTER";
        case EventType::GEOFENCE_EXIT: return "GEOFENCE_EXIT";
        case EventType::SPEED_LIMIT_EXCEEDED: return "SPEED_LIMIT_EXCEEDED";
        case EventType::LOW_BATTERY: return "LOW_BATTERY";
        case EventType::POWER_ON: return "POWER_ON";
        case EventType::POWER_OFF: return "POWER_OFF";
        case EventType::EXTERNAL_POWER_CONNECTED: return "EXTERNAL_POWER_CONNECTED";
        case EventType::EXTERNAL_POWER_DISCONNECTED: return "EXTERNAL_POWER_DISCONNECTED";
        case EventType::TEMPERATURE_ALERT: return "TEMPERATURE_ALERT";
        default: return "INVALID";
    }
}

/**
 * @brief Parse MessageType from string
 * @param str String to parse
 * @return MessageType value or UNKNOWN if not found
 */
inline MessageType messageTypeFromString(const std::string& str) {
    if (str == "GPS_UPDATE") return MessageType::GPS_UPDATE;
    if (str == "STATUS_UPDATE") return MessageType::STATUS_UPDATE;
    if (str == "EVENT_NOTIFICATION") return MessageType::EVENT_NOTIFICATION;
    if (str == "COMMAND_ACK") return MessageType::COMMAND_ACK;
    if (str == "COMMAND_REBOOT") return MessageType::COMMAND_REBOOT;
    if (str == "COMMAND_SUBSCRIBE") return MessageType::COMMAND_SUBSCRIBE;
    if (str == "COMMAND_UNSUBSCRIBE") return MessageType::COMMAND_UNSUBSCRIBE;
    if (str == "COMMAND_UPDATE_CONFIG") return MessageType::COMMAND_UPDATE_CONFIG;
    if (str == "HEARTBEAT") return MessageType::HEARTBEAT;
    if (str == "DEBUG_INFO") return MessageType::DEBUG_INFO;
    return MessageType::UNKNOWN;
}

/**
 * @brief Parse CommandType from string
 * @param str String to parse
 * @return CommandType value or REBOOT if not found
 */
inline CommandType commandTypeFromString(const std::string& str) {
    if (str == "REBOOT") return CommandType::REBOOT;
    if (str == "SUBSCRIBE") return CommandType::SUBSCRIBE;
    if (str == "UNSUBSCRIBE") return CommandType::UNSUBSCRIBE;
    if (str == "UPDATE_CONFIG") return CommandType::UPDATE_CONFIG;
    if (str == "GET_STATUS") return CommandType::GET_STATUS;
    if (str == "SET_INTERVAL") return CommandType::SET_INTERVAL;
    if (str == "FACTORY_RESET") return CommandType::FACTORY_RESET;
    return CommandType::REBOOT;
}

/**
 * @brief Check if a MessageType is a command type
 * @param type MessageType to check
 * @return true if the message type represents a command
 */
inline bool isCommandMessage(MessageType type) {
    return type == MessageType::COMMAND_REBOOT ||
           type == MessageType::COMMAND_SUBSCRIBE ||
           type == MessageType::COMMAND_UNSUBSCRIBE ||
           type == MessageType::COMMAND_UPDATE_CONFIG;
}

/**
 * @brief Check if a string is a valid IMEI
 * @param imei String to validate
 * @return true if the string is a valid IMEI format
 */
inline bool isValidImei(const std::string& imei) {
    if (imei.length() != Constants::IMEI_LENGTH) {
        return false;
    }
    
    for (char c : imei) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Check if coordinates are within valid GPS ranges
 * @param latitude Latitude to check
 * @param longitude Longitude to check
 * @return true if coordinates are valid
 */
inline bool areValidGPSCoordinates(double latitude, double longitude) {
    return latitude >= Constants::MIN_LATITUDE && latitude <= Constants::MAX_LATITUDE &&
           longitude >= Constants::MIN_LONGITUDE && longitude <= Constants::MAX_LONGITUDE;
}

} // namespace protocol_parser