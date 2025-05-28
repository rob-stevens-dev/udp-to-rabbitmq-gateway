// include/rabbitmq-integration/channel.hpp
#pragma once

#include "types.hpp"
#include "message.hpp"
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <unordered_map>

extern "C" {
#include <amqp.h>
}

namespace rabbitmq {

class Connection;

class Channel : public std::enable_shared_from_this<Channel> {
public:
    // Constructor (should only be called by Connection)
    Channel(std::shared_ptr<Connection> connection, int channelId);
    
    // Destructor
    ~Channel();
    
    // Non-copyable, non-movable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;
    
    // Channel management
    Result<void> open();
    Result<void> close();
    bool isOpen() const;
    ChannelState getState() const;
    int getChannelId() const;
    
    // Exchange operations
    Result<void> declareExchange(const ExchangeConfig& config);
    Result<void> deleteExchange(const std::string& name, bool ifUnused = false);
    Result<bool> exchangeExists(const std::string& name);
    
    // Queue operations
    Result<std::string> declareQueue(const QueueConfig& config);
    Result<void> deleteQueue(const std::string& name, bool ifUnused = false, bool ifEmpty = false);
    Result<void> purgeQueue(const std::string& name);
    Result<QueueInfo> getQueueInfo(const std::string& name);
    Result<bool> queueExists(const std::string& name);
    
    // Binding operations
    Result<void> bindQueue(const BindingConfig& config);
    Result<void> unbindQueue(const std::string& queue, const std::string& exchange, 
                            const std::string& routingKey);
    
    // Publishing
    Result<void> basicPublish(const std::string& exchange, const std::string& routingKey,
                             const Message& message, bool mandatory = false, bool immediate = false);
    
    // Publisher confirms
    Result<void> enableConfirms();
    Result<bool> waitForConfirm(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void setConfirmCallback(std::function<void(uint64_t deliveryTag, bool ack)> callback);
    
    // Return handling
    void setReturnCallback(std::function<void(const std::string& exchange, const std::string& routingKey,
                                            const Message& message, const std::string& replyText)> callback);
    
    // Consuming
    Result<std::string> basicConsume(const std::string& queue, MessageCallback callback,
                                    const std::string& consumerTag = "", bool noLocal = false,
                                    bool noAck = false, bool exclusive = false);
    Result<void> basicCancel(const std::string& consumerTag);
    
    // Message acknowledgment
    Result<void> basicAck(uint64_t deliveryTag, bool multiple = false);
    Result<void> basicNack(uint64_t deliveryTag, bool multiple = false, bool requeue = true);
    Result<void> basicReject(uint64_t deliveryTag, bool requeue = true);
    
    // Basic get (polling)
    Result<std::optional<Envelope>> basicGet(const std::string& queue, bool noAck = false);
    
    // Quality of Service
    Result<void> basicQos(uint16_t prefetchCount, uint32_t prefetchSize = 0, bool global = false);
    
    // Transactions
    Result<void> txSelect();
    Result<void> txCommit();
    Result<void> txRollback();
    
    // Statistics
    ChannelStats getStats() const;
    void resetStats();
    
    // Flow control
    Result<void> flow(bool active);
    
private:
    // Connection reference
    std::weak_ptr<Connection> connection_;
    int channelId_;
    
    // State
    mutable std::mutex mutex_;
    std::atomic<ChannelState> state_;
    
    // Consumer management
    std::unordered_map<std::string, MessageCallback> consumers_;
    std::thread consumerThread_;
    std::atomic<bool> stopConsuming_;
    
    // Publisher confirms
    bool confirmsEnabled_;
    std::atomic<uint64_t> nextPublishSeqNo_;
    std::unordered_map<uint64_t, std::promise<bool>> pendingConfirms_;
    std::function<void(uint64_t, bool)> confirmCallback_;
    
    // Return handling
    std::function<void(const std::string&, const std::string&, const Message&, const std::string&)> returnCallback_;
    
    // Statistics
    mutable ChannelStats stats_;
    
    // Transaction state
    bool inTransaction_;
    
    // Internal methods
    Result<void> doOpen();
    Result<void> doClose();
    void setState(ChannelState newState);
    void consumerLoop();
    void processFrame(const amqp_frame_t& frame);
    void processMethod(const amqp_frame_t& frame);
    void processContent(const amqp_frame_t& frame);
    void handleBasicDeliver(const amqp_basic_deliver_t& deliver, const Message& message);
    void handleBasicReturn(const amqp_basic_return_t& basicReturn, const Message& message);
    void handleBasicAck(const amqp_basic_ack_t& ack);
    void handleBasicNack(const amqp_basic_nack_t& nack);
    
    // Helper methods
    amqp_connection_state_t getConnection();
    Message frameToMessage(const amqp_frame_t& frame);
    amqp_basic_properties_t messageToProperties(const Message& message);
    Result<void> handleAmqpError(const std::string& operation, amqp_rpc_reply_t reply);
    
    friend class Connection;
};

} // namespace rabbitmq