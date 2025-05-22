// components/redis-deduplication/test/unit/test_message_id.cpp
#include <gtest/gtest.h>
#include "redis_deduplication/types.hpp"
#include <set>
#include <unordered_set>

using namespace redis_deduplication;

class MessageIdTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageIdTest, DefaultConstruction) {
    MessageId msg;
    EXPECT_TRUE(msg.imei.empty());
    EXPECT_EQ(0, msg.sequenceNumber);
}

TEST_F(MessageIdTest, ParameterizedConstruction) {
    std::string testImei = "123456789012345";
    uint32_t testSeqNum = 12345;
    
    MessageId msg(testImei, testSeqNum);
    EXPECT_EQ(testImei, msg.imei);
    EXPECT_EQ(testSeqNum, msg.sequenceNumber);
}

TEST_F(MessageIdTest, CopyConstruction) {
    MessageId original("123456789012345", 999);
    MessageId copy(original);
    
    EXPECT_EQ(original.imei, copy.imei);
    EXPECT_EQ(original.sequenceNumber, copy.sequenceNumber);
    EXPECT_TRUE(original == copy);
}

TEST_F(MessageIdTest, AssignmentOperator) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2;
    
    msg2 = msg1;
    EXPECT_EQ(msg1.imei, msg2.imei);
    EXPECT_EQ(msg1.sequenceNumber, msg2.sequenceNumber);
    EXPECT_TRUE(msg1 == msg2);
}

TEST_F(MessageIdTest, EqualityOperator) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 100);
    MessageId msg3("123456789012345", 101);
    MessageId msg4("987654321098765", 100);
    
    // Same IMEI and sequence number
    EXPECT_TRUE(msg1 == msg2);
    
    // Same IMEI, different sequence number
    EXPECT_FALSE(msg1 == msg3);
    
    // Different IMEI, same sequence number
    EXPECT_FALSE(msg1 == msg4);
    
    // Different IMEI and sequence number
    EXPECT_FALSE(msg3 == msg4);
}

TEST_F(MessageIdTest, LessThanOperator) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    MessageId msg3("987654321098765", 50);
    MessageId msg4("987654321098765", 200);
    
    // Same IMEI, different sequence numbers
    EXPECT_TRUE(msg1 < msg2);
    EXPECT_FALSE(msg2 < msg1);
    
    // Different IMEIs - lexicographic comparison
    EXPECT_TRUE(msg1 < msg3);  // "123..." < "987..."
    EXPECT_TRUE(msg2 < msg4);
    EXPECT_FALSE(msg3 < msg1);
    EXPECT_FALSE(msg4 < msg2);
    
    // Same MessageId should not be less than itself
    EXPECT_FALSE(msg1 < msg1);
}

TEST_F(MessageIdTest, RedisKeyGeneration) {
    MessageId msg("123456789012345", 42);
    
    // Default prefix
    std::string defaultKey = msg.toRedisKey();
    EXPECT_EQ("dedup:123456789012345:42", defaultKey);
    
    // Custom prefix
    std::string customKey = msg.toRedisKey("custom_prefix");
    EXPECT_EQ("custom_prefix:123456789012345:42", customKey);
    
    // Empty prefix
    std::string emptyPrefixKey = msg.toRedisKey("");
    EXPECT_EQ(":123456789012345:42", emptyPrefixKey);
}

TEST_F(MessageIdTest, RedisKeyWithLargeSequenceNumber) {
    MessageId msg("123456789012345", 0xFFFFFFFF); // Max uint32_t
    
    std::string key = msg.toRedisKey();
    EXPECT_EQ("dedup:123456789012345:4294967295", key);
}

TEST_F(MessageIdTest, RedisKeyWithZeroSequenceNumber) {
    MessageId msg("123456789012345", 0);
    
    std::string key = msg.toRedisKey();
    EXPECT_EQ("dedup:123456789012345:0", key);
}

TEST_F(MessageIdTest, ContainerUsage) {
    // Test usage in std::set (requires operator<)
    std::set<MessageId> messageSet;
    
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    MessageId msg3("987654321098765", 100);
    MessageId msg4("123456789012345", 100); // Duplicate of msg1
    
    messageSet.insert(msg1);
    messageSet.insert(msg2);
    messageSet.insert(msg3);
    messageSet.insert(msg4); // Should not increase size
    
    EXPECT_EQ(3, messageSet.size());
    EXPECT_TRUE(messageSet.find(msg1) != messageSet.end());
    EXPECT_TRUE(messageSet.find(msg2) != messageSet.end());
    EXPECT_TRUE(messageSet.find(msg3) != messageSet.end());
    EXPECT_TRUE(messageSet.find(msg4) != messageSet.end()); // Same as msg1
}

TEST_F(MessageIdTest, SortingBehavior) {
    std::vector<MessageId> messages = {
        MessageId("987654321098765", 100),
        MessageId("123456789012345", 200),
        MessageId("123456789012345", 100),
        MessageId("555555555555555", 150),
        MessageId("987654321098765", 50)
    };
    
    std::sort(messages.begin(), messages.end());
    
    // Check that sorting works correctly
    // Should be sorted by IMEI first, then by sequence number
    EXPECT_EQ("123456789012345", messages[0].imei);
    EXPECT_EQ(100, messages[0].sequenceNumber);
    
    EXPECT_EQ("123456789012345", messages[1].imei);
    EXPECT_EQ(200, messages[1].sequenceNumber);
    
    EXPECT_EQ("555555555555555", messages[2].imei);
    EXPECT_EQ(150, messages[2].sequenceNumber);
    
    EXPECT_EQ("987654321098765", messages[3].imei);
    EXPECT_EQ(50, messages[3].sequenceNumber);
    
    EXPECT_EQ("987654321098765", messages[4].imei);
    EXPECT_EQ(100, messages[4].sequenceNumber);
}

TEST_F(MessageIdTest, EdgeCaseIMEIs) {
    // Test with edge case IMEIs
    MessageId msg1("000000000000000", 1);
    MessageId msg2("999999999999999", 1);
    MessageId msg3("123456789012345", 1);
    
    std::string key1 = msg1.toRedisKey();
    std::string key2 = msg2.toRedisKey();
    std::string key3 = msg3.toRedisKey();
    
    EXPECT_EQ("dedup:000000000000000:1", key1);
    EXPECT_EQ("dedup:999999999999999:1", key2);
    EXPECT_EQ("dedup:123456789012345:1", key3);
    
    // Test ordering
    EXPECT_TRUE(msg1 < msg3);
    EXPECT_TRUE(msg3 < msg2);
}

TEST_F(MessageIdTest, LargeScaleComparison) {
    // Test with many MessageIds to ensure consistent ordering
    std::vector<MessageId> messages;
    
    // Generate test data
    for (int i = 0; i < 1000; ++i) {
        std::string imei = "12345678901234" + std::to_string(i % 10);
        uint32_t seqNum = i % 100;
        messages.emplace_back(imei, seqNum);
    }
    
    // Sort them
    std::sort(messages.begin(), messages.end());
    
    // Verify they're actually sorted
    for (size_t i = 1; i < messages.size(); ++i) {
        EXPECT_FALSE(messages[i] < messages[i-1]) 
            << "Messages not properly sorted at index " << i;
    }
}

TEST_F(MessageIdTest, ComparisonConsistency) {
    MessageId msg1("123456789012345", 100);
    MessageId msg2("123456789012345", 101);
    MessageId msg3("987654321098765", 100);
    
    // Test transitivity: if a < b and b < c, then a < c
    EXPECT_TRUE(msg1 < msg2);  // a < b
    EXPECT_TRUE(msg2 < msg3);  // b < c (different IMEI comparison)
    // Note: Due to IMEI lexicographic ordering, msg1 < msg3 should be true
    EXPECT_TRUE(msg1 < msg3);  // a < c
    
    // Test antisymmetry: if a < b, then !(b < a)
    EXPECT_TRUE(msg1 < msg2);
    EXPECT_FALSE(msg2 < msg1);
    
    // Test irreflexivity: !(a < a)
    EXPECT_FALSE(msg1 < msg1);
    EXPECT_FALSE(msg2 < msg2);
    EXPECT_FALSE(msg3 < msg3);
}