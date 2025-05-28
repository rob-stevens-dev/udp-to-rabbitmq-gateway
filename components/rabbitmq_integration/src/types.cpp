// src/types.cpp
#include "rabbitmq_integration/types.hpp"
#include <sstream>
#include <cstring>

namespace rabbitmq_integration {

// Utility function implementations
std::string exchangeTypeToString(rabbitmq_integration::ExchangeType type) {
    switch (type) {
        case rabbitmq_integration::ExchangeType::Direct: return "direct";
        case rabbitmq_integration::ExchangeType::Fanout: return "fanout";
        case rabbitmq_integration::ExchangeType::Topic: return "topic";
        case rabbitmq_integration::ExchangeType::Headers: return "headers";
        default: return "unknown";
    }
}

rabbitmq_integration::ExchangeType stringToExchangeType(const std::string& str) {
    if (str == "direct") return rabbitmq_integration::ExchangeType::Direct;
    if (str == "fanout") return rabbitmq_integration::ExchangeType::Fanout;
    if (str == "topic") return rabbitmq_integration::ExchangeType::Topic;
    if (str == "headers") return rabbitmq_integration::ExchangeType::Headers;
    return rabbitmq_integration::ExchangeType::Direct; // Default
}

std::string connectionStateToString(rabbitmq_integration::ConnectionState state) {
    switch (state) {
        case rabbitmq_integration::ConnectionState::Disconnected: return "Disconnected";
        case rabbitmq_integration::ConnectionState::Connecting: return "Connecting";
        case rabbitmq_integration::ConnectionState::Connected: return "Connected";
        case rabbitmq_integration::ConnectionState::Disconnecting: return "Disconnecting";
        case rabbitmq_integration::ConnectionState::Failed: return "Failed";
        default: return "Unknown";
    }
}

std::string channelStateToString(rabbitmq_integration::ChannelState state) {
    switch (state) {
        case rabbitmq_integration::ChannelState::Closed: return "Closed";
        case rabbitmq_integration::ChannelState::Opening: return "Opening";
        case rabbitmq_integration::ChannelState::Open: return "Open";
        case rabbitmq_integration::ChannelState::Closing: return "Closing";
        case rabbitmq_integration::ChannelState::Failed: return "Failed";
        default: return "Unknown";
    }
}

std::string errorTypeToString(rabbitmq_integration::ErrorType error) {
    switch (error) {
        case rabbitmq_integration::ErrorType::None: return "None";
        case rabbitmq_integration::ErrorType::ConnectionError: return "ConnectionError";
        case rabbitmq_integration::ErrorType::ChannelError: return "ChannelError";
        case rabbitmq_integration::ErrorType::AuthenticationError: return "AuthenticationError";
        case rabbitmq_integration::ErrorType::NetworkError: return "NetworkError";
        case rabbitmq_integration::ErrorType::ProtocolError: return "ProtocolError";
        case rabbitmq_integration::ErrorType::TimeoutError: return "TimeoutError";
        case rabbitmq_integration::ErrorType::ResourceError: return "ResourceError";
        default: return "Unknown";
    }
}

std::string amqpErrorToString(int error) {
    switch (error) {
        case AMQP_STATUS_OK: return "OK";
        case AMQP_STATUS_NO_MEMORY: return "No memory";
        case AMQP_STATUS_BAD_AMQP_DATA: return "Bad AMQP data";
        case AMQP_STATUS_UNKNOWN_CLASS: return "Unknown class";
        case AMQP_STATUS_UNKNOWN_METHOD: return "Unknown method";
        case AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED: return "Hostname resolution failed";
        case AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION: return "Incompatible AMQP version";
        case AMQP_STATUS_CONNECTION_CLOSED: return "Connection closed";
        case AMQP_STATUS_BAD_URL: return "Bad URL";
        case AMQP_STATUS_SOCKET_ERROR: return "Socket error";
        case AMQP_STATUS_INVALID_PARAMETER: return "Invalid parameter";
        case AMQP_STATUS_TABLE_TOO_BIG: return "Table too big";
        case AMQP_STATUS_WRONG_METHOD: return "Wrong method";
        case AMQP_STATUS_TIMEOUT: return "Timeout";
        case AMQP_STATUS_TIMER_FAILURE: return "Timer failure";
        case AMQP_STATUS_HEARTBEAT_TIMEOUT: return "Heartbeat timeout";
        case AMQP_STATUS_UNEXPECTED_STATE: return "Unexpected state";
        case AMQP_STATUS_SOCKET_CLOSED: return "Socket closed";
        case AMQP_STATUS_SOCKET_INUSE: return "Socket in use";
        case AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD: return "Broker unsupported SASL method";
        case AMQP_STATUS_UNSUPPORTED: return "Unsupported";
        default:
            return "Unknown error (" + std::to_string(error) + ")";
    }
}

rabbitmq_integration::ErrorType amqpErrorToErrorType(int error) {
    switch (error) {
        case AMQP_STATUS_OK:
            return rabbitmq_integration::ErrorType::None;
        case AMQP_STATUS_NO_MEMORY:
        case AMQP_STATUS_TABLE_TOO_BIG:
            return rabbitmq_integration::ErrorType::ResourceError;
        case AMQP_STATUS_BAD_AMQP_DATA:
        case AMQP_STATUS_UNKNOWN_CLASS:
        case AMQP_STATUS_UNKNOWN_METHOD:
        case AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION:
        case AMQP_STATUS_WRONG_METHOD:
        case AMQP_STATUS_UNEXPECTED_STATE:
            return rabbitmq_integration::ErrorType::ProtocolError;
        case AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED:
        case AMQP_STATUS_SOCKET_ERROR:
        case AMQP_STATUS_SOCKET_CLOSED:
        case AMQP_STATUS_SOCKET_INUSE:
            return rabbitmq_integration::ErrorType::NetworkError;
        case AMQP_STATUS_CONNECTION_CLOSED:
            return rabbitmq_integration::ErrorType::ConnectionError;
        case AMQP_STATUS_TIMEOUT:
        case AMQP_STATUS_HEARTBEAT_TIMEOUT:
        case AMQP_STATUS_TIMER_FAILURE:
            return rabbitmq_integration::ErrorType::TimeoutError;
        case AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD:
            return rabbitmq_integration::ErrorType::AuthenticationError;
        case AMQP_STATUS_BAD_URL:
        case AMQP_STATUS_INVALID_PARAMETER:
        case AMQP_STATUS_UNSUPPORTED:
        default:
            return rabbitmq_integration::ErrorType::ProtocolError;
    }
}

amqp_table_t stringMapToAmqpTable(const std::map<std::string, std::string>& map) {
    amqp_table_t table;
    table.num_entries = map.size();
    
    if (map.empty()) {
        table.entries = nullptr;
        return table;
    }
    
    table.entries = static_cast<amqp_table_entry_t*>(
        malloc(sizeof(amqp_table_entry_t) * map.size()));
    
    size_t i = 0;
    for (const auto& [key, value] : map) {
        table.entries[i].key = amqp_cstring_bytes(key.c_str());
        table.entries[i].value.kind = AMQP_FIELD_KIND_UTF8;
        table.entries[i].value.value.bytes = amqp_cstring_bytes(value.c_str());
        ++i;
    }
    
    return table;
}

std::map<std::string, std::string> amqpTableToStringMap(const amqp_table_t& table) {
    std::map<std::string, std::string> map;
    
    for (int i = 0; i < table.num_entries; ++i) {
        const auto& entry = table.entries[i];
        
        std::string key(static_cast<char*>(entry.key.bytes), entry.key.len);
        
        std::string value;
        if (entry.value.kind == AMQP_FIELD_KIND_UTF8 || 
            entry.value.kind == AMQP_FIELD_KIND_BYTES) {
            value = std::string(static_cast<char*>(entry.value.value.bytes.bytes), 
                               entry.value.value.bytes.len);
        } else {
            // Convert other types to string
            switch (entry.value.kind) {
                case AMQP_FIELD_KIND_BOOLEAN:
                    value = entry.value.value.boolean ? "true" : "false";
                    break;
                case AMQP_FIELD_KIND_I8:
                    value = std::to_string(entry.value.value.i8);
                    break;
                case AMQP_FIELD_KIND_U8:
                    value = std::to_string(entry.value.value.u8);
                    break;
                case AMQP_FIELD_KIND_I16:
                    value = std::to_string(entry.value.value.i16);
                    break;
                case AMQP_FIELD_KIND_U16:
                    value = std::to_string(entry.value.value.u16);
                    break;
                case AMQP_FIELD_KIND_I32:
                    value = std::to_string(entry.value.value.i32);
                    break;
                case AMQP_FIELD_KIND_U32:
                    value = std::to_string(entry.value.value.u32);
                    break;
                case AMQP_FIELD_KIND_I64:
                    value = std::to_string(entry.value.value.i64);
                    break;
                case AMQP_FIELD_KIND_U64:
                    value = std::to_string(entry.value.value.u64);
                    break;
                case AMQP_FIELD_KIND_F32:
                    value = std::to_string(entry.value.value.f32);
                    break;
                case AMQP_FIELD_KIND_F64:
                    value = std::to_string(entry.value.value.f64);
                    break;
                default:
                    value = "";
                    break;
            }
        }
        
        map[key] = value;
    }
    
    return map;
}

// Exception implementations
rabbitmq_integration::RabbitMQException::RabbitMQException(const std::string& message, ErrorType type)
    : message_(message), errorType_(type) {
}

const char* rabbitmq_integration::RabbitMQException::what() const noexcept {
    return message_.c_str();
}

rabbitmq_integration::ErrorType rabbitmq_integration::RabbitMQException::getErrorType() const noexcept {
    return errorType_;
}

rabbitmq_integration::ConnectionException::ConnectionException(const std::string& message)
    : RabbitMQException(message, ErrorType::ConnectionError) {
}

rabbitmq_integration::ChannelException::ChannelException(const std::string& message)
    : RabbitMQException(message, ErrorType::ChannelError) {
}

rabbitmq_integration::AuthenticationException::AuthenticationException(const std::string& message)
    : RabbitMQException(message, ErrorType::AuthenticationError) {
}

rabbitmq_integration::NetworkException::NetworkException(const std::string& message)
    : RabbitMQException(message, ErrorType::NetworkError) {
}

rabbitmq_integration::TimeoutException::TimeoutException(const std::string& message)
    : RabbitMQException(message, ErrorType::TimeoutError) {
}

} // namespace rabbitmq