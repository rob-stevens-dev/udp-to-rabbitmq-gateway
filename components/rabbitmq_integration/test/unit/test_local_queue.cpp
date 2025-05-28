// test/unit/test_local_queue.cpp
#include <gtest/gtest.h>
#include "rabbitmq_integration/local_queue.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <vector>

using namespace rabbitmq_integration;
using namespace rabbitmq_integration::test;

class LocalQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        queue_ = std::make_unique<LocalQueue>(5); // Small queue for testing
        testMessage_ = TestMessages::createTestTelemetryMessage();
    }

protected:
    std::unique_ptr<LocalQueue> queue_;
    Message testMessage_;
};

TEST_F(LocalQueueTest, DefaultState) {
    EXPECT_TRUE(queue_->isEmpty());
    EXPECT_FALSE(queue_->isFull());
    EXPECT_EQ(0, queue_->size());
    EXPECT_EQ(5, queue_->capacity());
}

TEST_F(LocalQueueTest, BasicEnqueueDequeue) {
    std::string exchange = "test-exchange";
    std::string routingKey = "test-key";
    
    // Enqueue
    EXPECT_TRUE(queue_->enqueue(exchange, routingKey, testMessage_));
    EXPECT_FALSE(queue_->isEmpty());
    EXPECT_EQ(1, queue_->size());
    
    // Dequeue
    auto item = queue_->dequeue();
    EXPECT_TRUE(item.has_value());
    
    auto [retExchange, retRoutingKey, retMessage] = *item;
    EXPECT_EQ(exchange, retExchange);
    EXPECT_EQ(routingKey, retRoutingKey);
    TestAssertions::assertMessageEquals(testMessage_, retMessage);
    
    EXPECT_TRUE(queue_->isEmpty());
    EXPECT_EQ(0, queue_->size());
}

TEST_F(LocalQueueTest, MultipleItems) {
    std::vector<std::tuple<std::string, std::string, Message>> items;
    
    // Enqueue multiple items
    for (int i = 0; i < 3; ++i) {
        std::string exchange = "exchange-" + std::to_string(i);
        std::string routingKey = "key-" + std::to_string(i);
        auto message = TestMessages::createTestTelemetryMessage("imei-" + std::to_string(i));
        
        items.emplace_back(exchange, routingKey, message);
        EXPECT_TRUE(queue_->enqueue(exchange, routingKey, message));
    }
    
    EXPECT_EQ(3, queue_->size());
    EXPECT_FALSE(queue_->isEmpty());
    EXPECT_FALSE(queue_->isFull());
    
    // Dequeue in FIFO order
    for (int i = 0; i < 3; ++i) {
        auto item = queue_->dequeue();
        EXPECT_TRUE(item.has_value());
        
        auto [retExchange, retRoutingKey, retMessage] = *item;
        auto [expectedExchange, expectedRoutingKey, expectedMessage] = items[i];
        
        EXPECT_EQ(expectedExchange, retExchange);
        EXPECT_EQ(expectedRoutingKey, retRoutingKey);
        TestAssertions::assertMessageEquals(expectedMessage, retMessage);
    }
    
    EXPECT_TRUE(queue_->isEmpty());
}

TEST_F(LocalQueueTest, CapacityLimits) {
    // Fill queue to capacity
    for (size_t i = 0; i < queue_->capacity(); ++i) {
        EXPECT_TRUE(queue_->enqueue("exchange", "key", testMessage_));
    }
    
    EXPECT_TRUE(queue_->isFull());
    EXPECT_EQ(queue_->capacity(), queue_->size());
    
    // Try to add one more (should fail)
    EXPECT_FALSE(queue_->enqueue("exchange", "key", testMessage_));
    EXPECT_EQ(queue_->capacity(), queue_->size());
}

TEST_F(LocalQueueTest, DequeueEmpty) {
    EXPECT_TRUE(queue_->isEmpty());
    
    auto item = queue_->dequeue();
    EXPECT_FALSE(item.has_value());
}

TEST_F(LocalQueueTest, Clear) {
    // Add some items
    for (int i = 0; i < 3; ++i) {
        queue_->enqueue("exchange", "key", testMessage_);
    }
    
    EXPECT_EQ(3, queue_->size());
    EXPECT_FALSE(queue_->isEmpty());
    
    queue_->clear();
    
    EXPECT_EQ(0, queue_->size());
    EXPECT_TRUE(queue_->isEmpty());
}

TEST_F(LocalQueueTest, SetMaxSize) {
    // Fill queue
    for (size_t i = 0; i < queue_->capacity(); ++i) {
        queue_->enqueue("exchange", "key", testMessage_);
    }
    EXPECT_EQ(5, queue_->size());
    
    // Reduce capacity
    queue_->setMaxSize(3);
    EXPECT_EQ(3, queue_->capacity());
    EXPECT_EQ(3, queue_->size()); // Should drop excess items
    
    // Increase capacity
    queue_->setMaxSize(10);
    EXPECT_EQ(10, queue_->capacity());
    EXPECT_EQ(3, queue_->size()); // Existing items remain
}
/*
TEST_F(LocalQueueTest, Statistics) {
    auto stats = queue_->getStats();
    EXPECT_EQ(0, stats.totalEnqueued);
    EXPECT_EQ(0, stats.totalDequeued);
    EXPECT_EQ(0, stats.totalDropped);
    EXPECT_EQ(0, stats.currentSize);
    EXPECT_EQ(5, stats.maxSize);
    EXPECT_EQ(0, stats.peakSize);
    
    // Enqueue some items
    for (int i = 0; i < 3; ++i) {
        queue_->enqueue("exchange", "key", testMessage_);
    }
    
    stats = queue_->getStats();
    EXPECT_EQ(3, stats.totalEnqueued);
    EXPECT_EQ(0, stats.totalDequeued);
    EXPECT_EQ(0, stats.totalDropped);
    EXPECT_EQ*/