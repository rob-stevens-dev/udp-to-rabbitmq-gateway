#include "protocol_parser/validator.hpp"
#include <algorithm>
#include <regex>

namespace protocol_parser {

Validator::Validator(const Config& config) : config_(config) {
    // Initialize validator with configuration
}

std::vector<ValidationErrorInfo> Validator::validateMessage(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Validate header
    auto headerErrors = validateHeader(message);
    errors.insert(errors.end(), headerErrors.begin(), headerErrors.end());
    
    // Validate fields
    auto fieldErrors = validateFields(message);
    errors.insert(errors.end(), fieldErrors.begin(), fieldErrors.end());
    
    return errors;
}

std::vector<ValidationErrorInfo> Validator::validateHeader(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Validate IMEI
    if (!validateImei(message.getImei())) {
        errors.emplace_back(
            "Invalid IMEI format: " + message.getImei(),
            "imei",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    }
    
    // Validate message type
    if (!validateMessageType(message.getType())) {
        errors.emplace_back(
            "Invalid message type: " + std::to_string(static_cast<int>(message.getType())),
            "messageType",
            ErrorSeverity::ERROR,
            ErrorCategory::PROTOCOL_ERROR
        );
    }
    
    // Validate protocol version
    if (config_.checkProtocolVersion && !validateProtocolVersion(message.getProtocolVersion())) {
        errors.emplace_back(
            "Unsupported protocol version: " + protocolVersionToString(message.getProtocolVersion()),
            "version",
            ErrorSeverity::WARNING,
            ErrorCategory::PROTOCOL_ERROR
        );
    }
    
    return errors;
}

std::vector<ValidationErrorInfo> Validator::validateFields(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Validate fields based on message type
    switch (message.getType()) {
        case MessageType::GPS_UPDATE:
            {
                auto gpsErrors = validateGPSFields(message);
                errors.insert(errors.end(), gpsErrors.begin(), gpsErrors.end());
            }
            break;
            
        case MessageType::STATUS_UPDATE:
            {
                auto statusErrors = validateStatusFields(message);
                errors.insert(errors.end(), statusErrors.begin(), statusErrors.end());
            }
            break;
            
        case MessageType::EVENT_NOTIFICATION:
            {
                auto eventErrors = validateEventFields(message);
                errors.insert(errors.end(), eventErrors.begin(), eventErrors.end());
            }
            break;
            
        case MessageType::COMMAND_ACK:
            {
                auto commandErrors = validateCommandFields(message);
                errors.insert(errors.end(), commandErrors.begin(), commandErrors.end());
            }
            break;
            
        default:
            if (config_.strictValidation) {
                errors.emplace_back(
                    "Unknown message type for field validation",
                    "messageType",
                    ErrorSeverity::WARNING,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
            break;
    }
    
    return errors;
}

bool Validator::validateImei(const std::string& imei) const {
    // IMEI should be exactly 15 digits
    if (imei.length() != 15) {
        return false;
    }
    
    // Check if all characters are digits
    return std::all_of(imei.begin(), imei.end(), ::isdigit);
}

bool Validator::validateProtocolVersion(const ProtocolVersion& version) const {
    // Support versions 1.0, 1.1, and 2.0
    if (version.major == 1) {
        return version.minor <= 1;
    } else if (version.major == 2) {
        return version.minor == 0;
    }
    return false;
}

bool Validator::validateMessageType(MessageType type) const {
    // Check if message type is within valid range
    int typeValue = static_cast<int>(type);
    return typeValue >= 1 && typeValue <= 10 && type != MessageType::UNKNOWN;
}

std::vector<ValidationErrorInfo> Validator::validateGPSFields(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Check required GPS fields
    if (!message.hasField("latitude")) {
        errors.emplace_back(
            "Required field 'latitude' missing",
            "latitude",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    } else if (config_.validateRanges) {
        try {
            double latitude = message.getField<double>("latitude");
            if (latitude < -90.0 || latitude > 90.0) {
                errors.emplace_back(
                    "Latitude out of valid range: " + std::to_string(latitude),
                    "latitude",
                    ErrorSeverity::ERROR,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
        } catch (const std::exception&) {
            errors.emplace_back(
                "Failed to access latitude field",
                "latitude",
                ErrorSeverity::ERROR,
                ErrorCategory::TYPE_ERROR
            );
        }
    }
    
    if (!message.hasField("longitude")) {
        errors.emplace_back(
            "Required field 'longitude' missing",
            "longitude",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    } else if (config_.validateRanges) {
        try {
            double longitude = message.getField<double>("longitude");
            if (longitude < -180.0 || longitude > 180.0) {
                errors.emplace_back(
                    "Longitude out of valid range: " + std::to_string(longitude),
                    "longitude",
                    ErrorSeverity::ERROR,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
        } catch (const std::exception&) {
            errors.emplace_back(
                "Failed to access longitude field",
                "longitude",
                ErrorSeverity::ERROR,
                ErrorCategory::TYPE_ERROR
            );
        }
    }
    
    // Validate optional speed field
    if (message.hasField("speed") && config_.validateRanges) {
        try {
            float speed = message.getField<float>("speed");
            if (speed < 0.0f || speed > 500.0f) { // Max 500 km/h
                errors.emplace_back(
                    "Speed out of valid range: " + std::to_string(speed),
                    "speed",
                    ErrorSeverity::WARNING,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
        } catch (const std::exception&) {
            errors.emplace_back(
                "Failed to access speed field",
                "speed",
                ErrorSeverity::WARNING,
                ErrorCategory::TYPE_ERROR
            );
        }
    }
    
    return errors;
}

std::vector<ValidationErrorInfo> Validator::validateStatusFields(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Validate battery level
    if (message.hasField("battery_level") && config_.validateRanges) {
        try {
            uint32_t batteryLevel = message.getField<uint32_t>("battery_level");
            if (batteryLevel > 100) {
                errors.emplace_back(
                    "Battery level out of valid range: " + std::to_string(batteryLevel),
                    "battery_level",
                    ErrorSeverity::WARNING,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
        } catch (const std::exception&) {
            errors.emplace_back(
                "Failed to access battery_level field",
                "battery_level",
                ErrorSeverity::WARNING,
                ErrorCategory::TYPE_ERROR
            );
        }
    }
    
    // Validate signal strength
    if (message.hasField("signal_strength") && config_.validateRanges) {
        try {
            int32_t signalStrength = message.getField<int32_t>("signal_strength");
            if (signalStrength < -120 || signalStrength > 0) { // dBm range
                errors.emplace_back(
                    "Signal strength out of valid range: " + std::to_string(signalStrength),
                    "signal_strength",
                    ErrorSeverity::WARNING,
                    ErrorCategory::VALIDATION_ERROR
                );
            }
        } catch (const std::exception&) {
            errors.emplace_back(
                "Failed to access signal_strength field",
                "signal_strength",
                ErrorSeverity::WARNING,
                ErrorCategory::TYPE_ERROR
            );
        }
    }
    
    return errors;
}

std::vector<ValidationErrorInfo> Validator::validateEventFields(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Check required event type field
    if (!message.hasField("event_type")) {
        errors.emplace_back(
            "Required field 'event_type' missing",
            "event_type",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    }
    
    return errors;
}

std::vector<ValidationErrorInfo> Validator::validateCommandFields(const Message& message) const {
    std::vector<ValidationErrorInfo> errors;
    
    // Check required command acknowledgment fields
    if (!message.hasField("original_sequence")) {
        errors.emplace_back(
            "Required field 'original_sequence' missing",
            "original_sequence",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    }
    
    if (!message.hasField("result_code")) {
        errors.emplace_back(
            "Required field 'result_code' missing",
            "result_code",
            ErrorSeverity::ERROR,
            ErrorCategory::VALIDATION_ERROR
        );
    }
    
    return errors;
}

std::string Validator::protocolVersionToString(const ProtocolVersion& version) const {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}

} // namespace protocol_parser