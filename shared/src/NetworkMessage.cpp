#include "NetworkMessage.hpp"
#include <cstring>

namespace inventory {

std::vector<uint8_t> NetworkMessage::serialize() const {
    std::vector<uint8_t> result;
    
    // message format: [type:1byte][payload_size:4bytes][payload:n bytes]
    result.push_back(static_cast<uint8_t>(type));
    
    uint32_t payloadSize = static_cast<uint32_t>(payload.size());
    result.push_back((payloadSize >> 24) & 0xFF);
    result.push_back((payloadSize >> 16) & 0xFF);
    result.push_back((payloadSize >> 8) & 0xFF);
    result.push_back(payloadSize & 0xFF);
    
    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

NetworkMessage NetworkMessage::deserialize(const std::vector<uint8_t>& data) {
    NetworkMessage msg;
    
    if (data.size() < 5) {
        return msg; // invalid message
    }
    
    msg.type = static_cast<MessageType>(data[0]);
    
    uint32_t payloadSize = (static_cast<uint32_t>(data[1]) << 24) |
                          (static_cast<uint32_t>(data[2]) << 16) |
                          (static_cast<uint32_t>(data[3]) << 8) |
                          static_cast<uint32_t>(data[4]);
    
    if (data.size() >= 5 + payloadSize) {
        msg.payload.assign(data.begin() + 5, data.begin() + 5 + payloadSize);
    }
    
    return msg;
}

} // namespace inventory
