#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace protocol_parser {

/**
 * @brief Base exception class for all protocol parsing errors
 */
class ProtocolError : public std::exception {
public:
    explicit ProtocolError(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

protected:
    std::string message_;
};

/**
 * @brief Exception thrown when parsing fails due to malformed data
 */
class ParseError : public ProtocolError {
public:
    explicit ParseError(const std::string& message) : ProtocolError("Parse error: " + message) {}
};

/**
 * @brief Exception thrown when field access fails
 */
class FieldAccessError : public ProtocolError {
public:
    explicit FieldAccessError(const std::string& message) : ProtocolError("Field access error: " + message) {}
};

/**
 * @brief Exception thrown when type conversion fails
 */
class TypeConversionError : public ProtocolError {
public:
    explicit TypeConversionError(const std::string& message) : ProtocolError("Type conversion error: " + message) {}
};

/**
 * @brief Exception thrown when validation fails
 */
class ValidationError : public ProtocolError {
public:
    explicit ValidationError(const std::string& message) : ProtocolError("Validation error: " + message) {}
};

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    WARNING,    ///< Non-fatal warning
    ERROR,      ///< Fatal error that prevents parsing
    CRITICAL    ///< Critical system error
};

/**
 * @brief Error categories for classification
 */
enum class ErrorCategory {
    PARSE_ERROR,        ///< Parsing/format errors
    VALIDATION_ERROR,   ///< Data validation errors
    TYPE_ERROR,         ///< Type conversion errors
    PROTOCOL_ERROR,     ///< Protocol-specific errors
    SYSTEM_ERROR        ///< System/infrastructure errors
};

/**
 * @brief Validation error information (non-exception)
 */
struct ValidationErrorInfo {
    std::string message;        ///< Human-readable error message
    std::string field;          ///< Field name that caused the error (if applicable)
    ErrorSeverity severity;     ///< Severity level of the error
    ErrorCategory category;     ///< Error category
    
    ValidationErrorInfo(const std::string& msg, const std::string& fld = "", 
                       ErrorSeverity sev = ErrorSeverity::ERROR, ErrorCategory cat = ErrorCategory::VALIDATION_ERROR)
        : message(msg), field(fld), severity(sev), category(cat) {}
};

/**
 * @brief Generic error information structure
 */
struct ErrorInfo {
    std::string message;
    std::string field;
    ErrorSeverity severity;
    ErrorCategory category;
    
    ErrorInfo(const std::string& msg, const std::string& fld = "",
             ErrorSeverity sev = ErrorSeverity::ERROR, ErrorCategory cat = ErrorCategory::VALIDATION_ERROR)
        : message(msg), field(fld), severity(sev), category(cat) {}
};

/**
 * @brief Error class for backwards compatibility with existing code
 */
class Error {
public:
    Error() : message_("No error"), severity_(ErrorSeverity::WARNING) {}
    explicit Error(const std::string& message, ErrorSeverity severity = ErrorSeverity::ERROR)
        : message_(message), severity_(severity) {}

    const std::string& getMessage() const { return message_; }
    ErrorSeverity getSeverity() const { return severity_; }
    const char* what() const noexcept { return message_.c_str(); }

private:
    std::string message_;
    ErrorSeverity severity_;
};

} // namespace protocol_parser