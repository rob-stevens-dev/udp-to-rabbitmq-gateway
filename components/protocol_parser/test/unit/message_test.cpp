#include <gtest/gtest.h>

#include "protocol_parser/message.hpp"
#include "protocol_parser/types.hpp"
#include "protocol_parser/error.hpp"

using namespace protocol_parser;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test message with the correct constructor signature
        testMessage = Message(MessageType::GPS_UPDATE, "123456789012345", 42);
        
        // Add test fields
        testMessage.addField("latitude", 37.7749);
        testMessage.addField("longitude", -122.4194);
        testMessage.addField("altitude", 10.5);
        testMessage.addField("accuracy", 5.0f);
        testMessage.addField("speed", 0.0f);
    }

    Message testMessage;
};

TEST_F(MessageTest, BasicProperties) {
    EXPECT_EQ(testMessage.getImei(), "123456789012345");
    EXPECT_EQ(testMessage.getSequence(), 42);
    EXPECT_EQ(testMessage.getType(), MessageType::GPS_UPDATE);
    
    ProtocolVersion version = testMessage.getProtocolVersion();
    EXPECT_EQ(version.major, 1);
    EXPECT_EQ(version.minor, 0);
    
    // Test protocol version setter
    ProtocolVersion newVersion{2, 1};
    testMessage.setProtocolVersion(newVersion);
    EXPECT_EQ(testMessage.getProtocolVersion().major, 2);
    EXPECT_EQ(testMessage.getProtocolVersion().minor, 1);
}

TEST_F(MessageTest, FieldOperations) {
    // Test field existence
    EXPECT_TRUE(testMessage.hasField("latitude"));
    EXPECT_FALSE(testMessage.hasField("nonexistent"));
    
    // Test field access
    double lat = testMessage.getField<double>("latitude");
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    
    float accuracy = testMessage.getField<float>("accuracy");
    EXPECT_FLOAT_EQ(accuracy, 5.0f);
    
    // Test adding duplicate field (should fail)
    EXPECT_FALSE(testMessage.addField("latitude", 99.0));
    
    // Test adding new field
    EXPECT_TRUE(testMessage.addField("heading", 90.0f));
    EXPECT_TRUE(testMessage.hasField("heading"));
    
    // Test removing field
    testMessage.removeField("heading");
    EXPECT_FALSE(testMessage.hasField("heading"));
    
    // Test removing non-existent field (should not crash)
    testMessage.removeField("nonexistent");
}

TEST_F(MessageTest, FieldTypeConversions) {
    testMessage.addField("boolField", true);
    testMessage.addField("int8Field", static_cast<int8_t>(-42));
    testMessage.addField("uint8Field", static_cast<uint8_t>(42));
    testMessage.addField("int16Field", static_cast<int16_t>(-1000));
    testMessage.addField("uint16Field", static_cast<uint16_t>(1000));
    testMessage.addField("int32Field", static_cast<int32_t>(-100000));
    testMessage.addField("uint32Field", static_cast<uint32_t>(100000));
    testMessage.addField("int64Field", static_cast<int64_t>(-10000000000LL));
    testMessage.addField("uint64Field", static_cast<uint64_t>(10000000000ULL));
    testMessage.addField("floatField", 3.14159f);
    testMessage.addField("doubleField", 3.14159265359);
    testMessage.addField("stringField", std::string("Hello, world!"));
    testMessage.addField("binaryField", std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04});

    // Test exact type retrieval
    EXPECT_EQ(testMessage.getField<int8_t>("int8Field"), -42);
    EXPECT_EQ(testMessage.getField<uint8_t>("uint8Field"), 42);
    EXPECT_EQ(testMessage.getField<int16_t>("int16Field"), -1000);
    EXPECT_EQ(testMessage.getField<uint16_t>("uint16Field"), 1000);
    EXPECT_EQ(testMessage.getField<int32_t>("int32Field"), -100000);
    EXPECT_EQ(testMessage.getField<uint32_t>("uint32Field"), 100000);
    EXPECT_EQ(testMessage.getField<int64_t>("int64Field"), -10000000000LL);
    EXPECT_EQ(testMessage.getField<uint64_t>("uint64Field"), 10000000000ULL);
    EXPECT_FLOAT_EQ(testMessage.getField<float>("floatField"), 3.14159f);
    EXPECT_DOUBLE_EQ(testMessage.getField<double>("doubleField"), 3.14159265359);
    EXPECT_EQ(testMessage.getField<std::string>("stringField"), "Hello, world!");
    
    auto binaryData = testMessage.getField<std::vector<uint8_t>>("binaryField");
    EXPECT_EQ(binaryData.size(), 4);
    EXPECT_EQ(binaryData[0], 0x01);
    EXPECT_EQ(binaryData[3], 0x04);

    // Test numeric conversions
    EXPECT_EQ(testMessage.getField<int>("int8Field"), -42);
    EXPECT_EQ(testMessage.getField<long>("int32Field"), -100000);
    EXPECT_FLOAT_EQ(testMessage.getField<float>("doubleField"), 3.14159265359f);

    // Test type-safe access
    auto optBool = testMessage.tryGetField<bool>("boolField");
    EXPECT_TRUE(optBool.has_value());
    EXPECT_TRUE(optBool.value());

    auto optNonexistent = testMessage.tryGetField<int>("nonexistent");
    EXPECT_FALSE(optNonexistent.has_value());

    auto optTypeMismatch = testMessage.tryGetField<std::string>("boolField");
    EXPECT_FALSE(optTypeMismatch.has_value());
}

TEST_F(MessageTest, BinaryConversion) {
    // Test binary serialization
    std::vector<uint8_t> binaryData = testMessage.toBinary();
    EXPECT_FALSE(binaryData.empty());
    EXPECT_GE(binaryData.size(), 25);  // Minimum header size

    // Test round-trip conversion
    Message parsedMessage = Message::fromBinary(binaryData);
    
    EXPECT_EQ(parsedMessage.getImei(), testMessage.getImei());
    EXPECT_EQ(parsedMessage.getSequence(), testMessage.getSequence());
    EXPECT_EQ(parsedMessage.getType(), testMessage.getType());
}

TEST_F(MessageTest, JsonConversion) {
    nlohmann::json jsonData = testMessage.toJson();
    
    EXPECT_EQ(jsonData["imei"], "123456789012345");
    EXPECT_EQ(jsonData["sequence"], 42);
    EXPECT_EQ(jsonData["messageType"], static_cast<int>(MessageType::GPS_UPDATE));
    EXPECT_EQ(jsonData["version"]["major"], 1);
    EXPECT_EQ(jsonData["version"]["minor"], 0);
    
    // Check that fields are present
    EXPECT_TRUE(jsonData.contains("fields"));
    EXPECT_TRUE(jsonData["fields"].contains("latitude"));
    EXPECT_DOUBLE_EQ(jsonData["fields"]["latitude"], 37.7749);
}

TEST_F(MessageTest, ValidationErrors) {
    // Test with invalid IMEI
    Message invalidMessage(MessageType::GPS_UPDATE, "12345", 1);  // Too short IMEI
    EXPECT_FALSE(invalidMessage.isValid());
    EXPECT_FALSE(invalidMessage.getValidationErrors().empty());
    
    // Test adding validation error
    ValidationErrorInfo error{
        "Test validation error",
        "test_field",
        ErrorSeverity::ERROR,
        ErrorCategory::VALIDATION_ERROR
    };
    
    testMessage.addValidationError(error);
    EXPECT_FALSE(testMessage.isValid());
    EXPECT_EQ(testMessage.getValidationErrors().size(), 1);
    EXPECT_EQ(testMessage.getValidationErrors()[0].message, "Test validation error");
    EXPECT_EQ(testMessage.getValidationErrors()[0].category, ErrorCategory::VALIDATION_ERROR);
}

TEST_F(MessageTest, FieldNames) {
    auto fieldNames = testMessage.getFieldNames();
    
    // Should be sorted alphabetically
    std::vector<std::string> expectedNames = {
        "accuracy", "altitude", "latitude", "longitude", "speed"
    };
    
    EXPECT_EQ(fieldNames.size(), expectedNames.size());
    for (size_t i = 0; i < expectedNames.size(); ++i) {
        EXPECT_EQ(fieldNames[i], expectedNames[i]);
    }
}

TEST_F(MessageTest, FieldAccessErrors) {
    // Test accessing non-existent field
    EXPECT_THROW(testMessage.getField<double>("nonexistent"), FieldAccessError);
    
    // Test type conversion error
    EXPECT_THROW(testMessage.getField<std::string>("latitude"), TypeConversionError);
}

TEST_F(MessageTest, DefaultConstructor) {
    Message defaultMessage;
    
    // Default message should be invalid
    EXPECT_FALSE(defaultMessage.isValid());
    EXPECT_EQ(defaultMessage.getImei(), "");
    EXPECT_EQ(defaultMessage.getSequence(), 0);
    EXPECT_EQ(defaultMessage.getType(), MessageType::UNKNOWN);
}

TEST_F(MessageTest, CopyAndMove) {
    // Test copy constructor
    Message copied = testMessage;
    EXPECT_EQ(copied.getImei(), testMessage.getImei());
    EXPECT_EQ(copied.getSequence(), testMessage.getSequence());
    EXPECT_EQ(copied.getType(), testMessage.getType());
    EXPECT_TRUE(copied.hasField("latitude"));
    
    // Test move constructor
    Message moved = std::move(copied);
    EXPECT_EQ(moved.getImei(), testMessage.getImei());
    EXPECT_EQ(moved.getSequence(), testMessage.getSequence());
    EXPECT_TRUE(moved.hasField("latitude"));
    
    // Test copy assignment
    Message assigned;
    assigned = testMessage;
    EXPECT_EQ(assigned.getImei(), testMessage.getImei());
    EXPECT_TRUE(assigned.hasField("latitude"));
}

// Test fixture for error conditions
class MessageErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create messages for error testing
    }
};

TEST_F(MessageErrorTest, InvalidBinaryData) {
    // Test with empty data
    std::vector<uint8_t> emptyData;
    EXPECT_THROW(Message::fromBinary(emptyData), ParseError);
    
    // Test with too short data
    std::vector<uint8_t> shortData = {0xAA, 0xBB, 0x01};
    EXPECT_THROW(Message::fromBinary(shortData), ParseError);
    
    // Test with invalid magic bytes
    std::vector<uint8_t> invalidMagic(25, 0);
    invalidMagic[0] = 0xFF;  // Wrong magic byte
    invalidMagic[1] = 0xBB;
    EXPECT_THROW(Message::fromBinary(invalidMagic), ParseError);
}