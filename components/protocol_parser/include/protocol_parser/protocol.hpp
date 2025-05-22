/**
 * @file protocol.hpp
 * @brief Protocol specification for device communication
 * 
 * This file defines the binary protocol format used for communication
 * between devices and the UDP Gateway.
 */

#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace protocol_parser {

/**
 * @brief Protocol constants
 */
namespace protocol {

/**
 * @brief Magic bytes used to identify protocol messages (0x55, 0xAA, 0x55, 0xAA)
 */
constexpr std::array<uint8_t, 4> MAGIC_BYTES = {0x55, 0xAA, 0x55, 0xAA};

/**
 * @brief Size of the protocol header in bytes
 */
constexpr size_t HEADER_SIZE = 9; // Magic(4) + Version(2) + Length(2) + Type(1)

/**
 * @brief Size of the IMEI field in bytes
 */
constexpr size_t IMEI_SIZE = 15;

/**
 * @brief Size of the sequence number field in bytes
 */
constexpr size_t SEQUENCE_SIZE = 4;

/**
 * @brief Size of the checksum field in bytes
 */
constexpr size_t CHECKSUM_SIZE = 4;

/**
 * @brief Maximum allowed packet size in bytes
 */
constexpr size_t MAX_PACKET_SIZE = 8192;

/**
 * @brief Message type values for device to server messages
 */
namespace message_type {
    constexpr uint8_t UNKNOWN      = 0x00;
    constexpr uint8_t GPS_UPDATE   = 0x01;
    constexpr uint8_t DEVICE_STATUS = 0x02;
    constexpr uint8_t EVENT        = 0x03;
    constexpr uint8_t CMD_ACK      = 0x04;
}

/**
 * @brief Command type values for server to device commands
 */
namespace command_type {
    constexpr uint8_t UNKNOWN     = 0x00;
    constexpr uint8_t SUBSCRIBE   = 0x81;
    constexpr uint8_t UNSUBSCRIBE = 0x82;
    constexpr uint8_t REBOOT      = 0x83;
    constexpr uint8_t CONFIG      = 0x84;
}

/**
 * @brief Event type values for EVENT messages
 */
namespace event_type {
    constexpr uint8_t UNKNOWN       = 0x00;
    constexpr uint8_t POWER_ON      = 0x01;
    constexpr uint8_t POWER_OFF     = 0x02;
    constexpr uint8_t LOW_BATTERY   = 0x03;
    constexpr uint8_t GEOFENCE      = 0x04;
    constexpr uint8_t MOTION        = 0x05;
    constexpr uint8_t SENSOR_ALERT  = 0x06;
}

/**
 * @brief Status code values for CMD_ACK messages
 */
namespace status_code {
    constexpr uint8_t SUCCESS       = 0x00;
    constexpr uint8_t INVALID_CMD   = 0x01;
    constexpr uint8_t INVALID_PARAM = 0x02;
    constexpr uint8_t NOT_SUPPORTED = 0x03;
    constexpr uint8_t INTERNAL_ERR  = 0x04;
    constexpr uint8_t TIMEOUT       = 0x05;
}

/**
 * @brief Binary protocol packet format
 * 
 * +----------------+------------------+----------------+---------------------+
 * | Header (9B)    | IMEI (15B)       | Sequence (4B)  | Payload (variable)  |
 * +----------------+------------------+----------------+---------------------+
 * | Magic    (4B)  |                  |                |                     |
 * | Version  (2B)  |                  |                |                     |
 * | Length   (2B)  |                  |                |                     |
 * | Type     (1B)  |                  |                |                     |
 * +----------------+------------------+----------------+---------------------+
 * 
 * Total message length includes the header, IMEI, sequence, payload, and checksum.
 * Checksum is calculated over the entire message excluding the checksum field itself.
 */

} // namespace protocol

} // namespace protocol_parser