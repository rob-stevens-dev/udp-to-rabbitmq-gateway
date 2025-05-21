#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace udp_gateway {

// IMEI type for device identification
using IMEI = std::string;

// Region code type
using RegionCode = std::string;

// Message sequence number type
using SequenceNumber = uint32_t;

// Message ID type
using MessageId = std::string;

// Priority level for messages
enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

// State of a device connection
enum class DeviceState {
    UNKNOWN,
    CONNECTED,
    DISCONNECTED,
    SUSPENDED
};

// Types of queues
enum class QueueType {
    INBOUND,   // Normal processing
    DEBUG,     // For diagnostics
    DISCARD,   // Invalid messages
    OUTBOUND   // Commands to devices
};

// Structure for basic metrics
struct GatewayMetrics {
    uint64_t totalPacketsReceived = 0;
    uint64_t totalPacketsProcessed = 0;
    uint64_t totalPacketsRejected = 0;
    uint64_t totalPacketsDuplicated = 0;
    uint64_t totalBytesReceived = 0;
    uint64_t activeConnections = 0;
    uint64_t peakConnections = 0;
    std::unordered_map<std::string, uint64_t> packetsPerRegion;
    std::unordered_map<std::string, uint64_t> errorsPerType;
    std::chrono::milliseconds averageProcessingTime{0};
    
    // Timestamp of last metrics update
    std::chrono::system_clock::time_point lastUpdated = std::chrono::system_clock::now();
};

// Structure representing a device message
struct DeviceMessage {
    IMEI deviceId;
    RegionCode region;
    std::vector<uint8_t> payload;
    SequenceNumber sequenceNumber = 0;
    MessageId messageId;
    Priority priority = Priority::NORMAL;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    bool requiresAcknowledgment = true;
};

// Structure for message acknowledgment
struct MessageAcknowledgment {
    IMEI deviceId;
    MessageId messageId;
    bool success = true;
    std::string errorReason;
};

// Structure for device command
struct DeviceCommand {
    IMEI deviceId;
    RegionCode region;
    std::vector<uint8_t> payload;
    MessageId commandId;
    Priority priority = Priority::NORMAL;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point expiresAt;
    bool delivered = false;
    bool acknowledged = false;
};

// Error codes
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_MESSAGE = 1,
    AUTHENTICATION_FAILURE = 2,
    RATE_LIMITED = 3,
    DUPLICATE_MESSAGE = 4,
    DEVICE_SUSPENDED = 5,
    INTERNAL_ERROR = 6,
    DEPENDENCY_FAILURE = 7,
    INVALID_CONFIGURATION = 8
};

// Convert ErrorCode to string
inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:               return "SUCCESS";
        case ErrorCode::INVALID_MESSAGE:       return "INVALID_MESSAGE";
        case ErrorCode::AUTHENTICATION_FAILURE: return "AUTHENTICATION_FAILURE";
        case ErrorCode::RATE_LIMITED:          return "RATE_LIMITED";
        case ErrorCode::DUPLICATE_MESSAGE:     return "DUPLICATE_MESSAGE";
        case ErrorCode::DEVICE_SUSPENDED:      return "DEVICE_SUSPENDED";
        case ErrorCode::INTERNAL_ERROR:        return "INTERNAL_ERROR";
        case ErrorCode::DEPENDENCY_FAILURE:    return "DEPENDENCY_FAILURE";
        case ErrorCode::INVALID_CONFIGURATION: return "INVALID_CONFIGURATION";
        default:                               return "UNKNOWN_ERROR";
    }
}

// Result structure for operations
template<typename T>
struct Result {
    bool success = false;
    ErrorCode errorCode = ErrorCode::SUCCESS;
    std::string errorMessage;
    T value;

    static Result<T> ok(const T& value) {
        Result<T> result;
        result.success = true;
        result.value = value;
        return result;
    }

    static Result<T> error(ErrorCode code, const std::string& message) {
        Result<T> result;
        result.success = false;
        result.errorCode = code;
        result.errorMessage = message;
        return result;
    }

    explicit operator bool() const {
        return success;
    }
};

// Void result for operations without return value
using VoidResult = Result<bool>;

inline VoidResult makeSuccessResult() {
    return Result<bool>::ok(true);
}

inline VoidResult makeErrorResult(ErrorCode code, const std::string& message) {
    return Result<bool>::error(code, message);
}

} // namespace udp_gateway