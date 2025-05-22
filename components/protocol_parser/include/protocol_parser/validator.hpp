#pragma once

#include "protocol_parser/types.hpp"
#include "protocol_parser/message.hpp"
#include "protocol_parser/error.hpp"
#include <vector>

namespace protocol_parser {

/**
 * @brief Message validator for protocol compliance and data integrity
 * 
 * This class provides validation functionality for parsed messages,
 * checking field values, protocol compliance, and data consistency.
 */
class Validator {
public:
    /**
     * @brief Validation configuration options
     */
    struct Config {
        bool strictValidation;       ///< Enable strict validation rules
        bool allowEmptyFields;       ///< Allow empty optional fields
        bool validateRanges;         ///< Validate field value ranges
        bool checkProtocolVersion;   ///< Validate protocol version compatibility
        
        Config()
            : strictValidation(true)
            , allowEmptyFields(false)
            , validateRanges(true)
            , checkProtocolVersion(true)
        {}
    };

    /**
     * @brief Constructor with configuration
     * @param config Validator configuration options
     */
    explicit Validator(const Config& config = Config{});

    /**
     * @brief Validate a complete message
     * @param message Message to validate
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<ValidationErrorInfo> validateMessage(const Message& message) const;

    /**
     * @brief Validate message header information
     * @param message Message to validate
     * @return Vector of validation errors for header
     */
    std::vector<ValidationErrorInfo> validateHeader(const Message& message) const;

    /**
     * @brief Validate message fields based on message type
     * @param message Message to validate
     * @return Vector of validation errors for fields
     */
    std::vector<ValidationErrorInfo> validateFields(const Message& message) const;

private:
    Config config_;

    // Validation helper methods
    bool validateImei(const std::string& imei) const;
    bool validateProtocolVersion(const ProtocolVersion& version) const;
    bool validateMessageType(MessageType type) const;
    
    // Field validation by message type
    std::vector<ValidationErrorInfo> validateGPSFields(const Message& message) const;
    std::vector<ValidationErrorInfo> validateStatusFields(const Message& message) const;
    std::vector<ValidationErrorInfo> validateEventFields(const Message& message) const;
    std::vector<ValidationErrorInfo> validateCommandFields(const Message& message) const;
    
    // Utility methods
    std::string protocolVersionToString(const ProtocolVersion& version) const;
};

} // namespace protocol_parser