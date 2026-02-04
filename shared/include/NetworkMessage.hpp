#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace inventory {

enum class MessageType : uint8_t {
    // Client to Server
    LOGIN_REQUEST = 1,
    DISCONNECT = 2,
    MOVE_ITEM_REQUEST = 10,
    SPLIT_STACK_REQUEST = 11,
    
    // Server to Client
    LOGIN_RESPONSE = 50,
    LOGIN_REJECTED = 51,
    INVENTORY_FULL_SYNC = 52,
    INVENTORY_UPDATE = 53,
    SHARED_STASH_UPDATE = 54,
    OPERATION_RESULT = 55,
    SERVER_SHUTDOWN = 56,
    
    // Bidirectional
    HEARTBEAT = 100
};

enum class LoginResult : uint8_t {
    SUCCESS = 0,
    USERNAME_ALREADY_CONNECTED = 1,
    INVALID_USERNAME = 2,
    SERVER_FULL = 3
};

enum class InventoryType : uint8_t {
    PERSONAL = 0,
    SHARED_STASH_1 = 1,
    SHARED_STASH_2 = 2,
    SHARED_STASH_3 = 3
};

struct NetworkMessage {
    MessageType type;
    std::vector<uint8_t> payload;
    
    NetworkMessage() : type(MessageType::HEARTBEAT) {}
    NetworkMessage(MessageType t) : type(t) {}
    
    // serialization helpers
    std::vector<uint8_t> serialize() const;
    static NetworkMessage deserialize(const std::vector<uint8_t>& data);
};

} // namespace inventory
