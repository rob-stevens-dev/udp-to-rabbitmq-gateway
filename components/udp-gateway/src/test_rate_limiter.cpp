#include "udp_gateway/rate_limiter.hpp"
#include <iostream>

int main() {
    auto limiter = udp_gateway::createDefaultRateLimiter();
    std::cout << "Rate limiter created successfully" << std::endl;
    return 0;
}
