#!/bin/bash
# This script creates the mock interface headers needed for building the UDP Gateway component

# Create the mock interfaces directory
mkdir -p include/mock_interfaces/protocol_parser
mkdir -p include/mock_interfaces/redis_deduplication
mkdir -p include/mock_interfaces/rabbitmq_integration
mkdir -p include/mock_interfaces/device_manager
mkdir -p include/mock_interfaces/monitoring_system

# Protocol Parser
cat > include/mock_interfaces/protocol_parser/protocol_parser.hpp << 'EOF'
#pragma once

#include <vector>
#include <string>
#include "udp_gateway/types.hpp"

namespace protocol_parser {
    class IProtocolParser {
    public:
        virtual ~IProtocolParser() = default;
        virtual udp_gateway::Result<udp_gateway::DeviceMessage> parseMessage(
            const std::vector<uint8_t>& data) = 0;
        virtual std::vector<uint8_t> createAcknowledgment(
            const udp_gateway::MessageAcknowledgment& ack) = 0;
        virtual std::vector<uint8_t> createCommandPacket(
            const std::vector<udp_gateway::DeviceCommand>& commands) = 0;
    };
}
EOF

# Redis Deduplication
cat > include/mock_interfaces/redis_deduplication/deduplication_service.hpp << 'EOF'
#pragma once

#include <string>
#include "udp_gateway/types.hpp"

namespace redis_deduplication {
    class IDeduplicationService {
    public:
        virtual ~IDeduplicationService() = default;
        virtual bool isDuplicate(
            const std::string& deviceId,
            uint32_t sequenceNumber,
            const std::string& messageId) = 0;
        virtual void recordMessage(
            const std::string& deviceId,
            uint32_t sequenceNumber,
            const std::string& messageId) = 0;
    };
}
EOF

# RabbitMQ Integration
cat > include/mock_interfaces/rabbitmq_integration/message_publisher.hpp << 'EOF'
#pragma once

#include <string>
#include <vector>

namespace rabbitmq_integration {
    class IMessagePublisher {
    public:
        virtual ~IMessagePublisher() = default;
        virtual bool publishMessage(
            const std::string& queueName,
            const std::vector<uint8_t>& data) = 0;
    };
}
EOF

# Device Manager
cat > include/mock_interfaces/device_manager/device_manager.hpp << 'EOF'
#pragma once

#include <string>
#include <vector>
#include "udp_gateway/types.hpp"

namespace device_manager {
    class IDeviceManager {
    public:
        virtual ~IDeviceManager() = default;
        virtual bool isAuthenticated(const udp_gateway::IMEI& deviceId) = 0;
        virtual udp_gateway::DeviceState getDeviceState(const udp_gateway::IMEI& deviceId) = 0;
        virtual void updateDeviceStatus(const udp_gateway::IMEI& deviceId, udp_gateway::DeviceState state) = 0;
        virtual std::vector<udp_gateway::DeviceCommand> getPendingCommands(const udp_gateway::IMEI& deviceId) = 0;
        virtual void markCommandDelivered(const std::string& commandId) = 0;
    };
}
EOF

# Monitoring System
cat > include/mock_interfaces/monitoring_system/metrics_collector.hpp << 'EOF'
#pragma once

#include <string>

namespace monitoring_system {
    class IMetricsCollector {
    public:
        virtual ~IMetricsCollector() = default;
        virtual void incrementCounter(const std::string& name, uint64_t value = 1) = 0;
        virtual void recordGauge(const std::string& name, double value) = 0;
        virtual void recordHistogram(const std::string& name, double value) = 0;
        virtual void recordError(const std::string& errorType) = 0;
    };
}
EOF

# Make the script executable
chmod +x include/mock_interfaces/setup-mock-interfaces.sh

echo "Mock interfaces created successfully"
echo "Include them in your build with -DCMAKE_INCLUDE_PATH=\"\$(pwd)/include/mock_interfaces\""