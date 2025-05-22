#pragma once

#include "protocol_parser/types.hpp"
#include "protocol_parser/message.hpp"
#include "protocol_parser/error.hpp"
#include <vector>
#include <cstdint>

namespace protocol_parser {

/**
 * @brief High-performance protocol parser for binary UDP packets
 * 
 * This class handles parsing of binary protocol messages from mobile devices,
 * extracting header information and payload data into structured Message objects.
 */
class Parser {
public:
    /**
     * @brief Parser configuration options
     */
    struct Config {
        bool strictMode;                ///< Enable strict validation
        bool allowLegacyProtocol;       ///< Support older protocol versions
        size_t maxPayloadSize;          ///< Maximum allowed payload size
        bool validateChecksums;         ///< Enable checksum validation
        bool allowUnknownMessageTypes;  ///< Allow parsing of unknown message types
        
        // Default constructor with default values
        Config() 
            : strictMode(true)
            , allowLegacyProtocol(false)
            , maxPayloadSize(4096)
            , validateChecksums(true)
            , allowUnknownMessageTypes(true)
        {}
    };

    /**
     * @brief Constructor with configuration
     * @param config Parser configuration options
     */
    explicit Parser(const Config& config = Config{});

    /**
     * @brief Parse a binary packet from a vector
     * @param packet Binary packet data
     * @return Parsed message object
     * @throws ParseError on parsing failure
     */
    Message parse(const std::vector<uint8_t>& packet);

    /**
     * @brief Parse a binary packet from raw memory
     * @param data Pointer to packet data
     * @param length Length of packet data
     * @return Parsed message object
     * @throws ParseError on parsing failure
     */
    Message parse(const uint8_t* data, size_t length);

    /**
     * @brief Get the last parsing error
     * @return Reference to the last error that occurred
     */
    const ParseError& getLastError() const { return lastError_; }

private:
    Config config_;
    mutable ParseError lastError_;

    // Protocol constants
    static constexpr size_t MINIMUM_PACKET_SIZE = 25;  ///< Minimum valid packet size
    static constexpr size_t HEADER_SIZE = 19;          ///< Fixed header size
    static constexpr size_t CHECKSUM_SIZE = 1;         ///< Checksum size

    /**
     * @brief Internal packet header structure
     */
    struct PacketHeader {
        ProtocolVersion version;
        MessageType type;
        std::string imei;
        uint32_t sequence;
    };

    /**
     * @brief Internal parsing implementation
     */
    Message parseImpl(const uint8_t* data, size_t length);

    /**
     * @brief Parse packet header
     */
    bool parseHeader(const uint8_t* data, size_t length, PacketHeader& header);

    /**
     * @brief Validate header data
     */
    void validateHeader(const PacketHeader& header, std::vector<ValidationErrorInfo>& errors);

    /**
     * @brief Parse message payload based on type
     */
    void parsePayload(const uint8_t* data, size_t length, MessageType type, Message& message);

    /**
     * @brief Parse GPS update message
     */
    void parseGPSUpdate(const uint8_t* data, size_t length, Message& message);

    /**
     * @brief Parse status update message
     */
    void parseStatusUpdate(const uint8_t* data, size_t length, Message& message);

    /**
     * @brief Parse event notification message
     */
    void parseEventNotification(const uint8_t* data, size_t length, Message& message);

    /**
     * @brief Parse command acknowledgment message
     */
    void parseCommandAck(const uint8_t* data, size_t length, Message& message);

    /**
     * @brief Verify packet checksum
     */
    bool verifyChecksum(const uint8_t* data, size_t length);
};

} // namespace protocol_parser