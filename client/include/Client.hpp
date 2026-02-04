#pragma once

#include "NetworkMessage.hpp"
#include "ClientInventory.hpp"
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>

namespace inventory {

class Client {
public:
    Client();
    ~Client();
    
    bool connect(const char* host, int port, const std::string& username);
    void disconnect();
    bool isConnected() const;
    
    const std::string& getUsername() const { return username_; }
    
    // Get inventory (thread-safe)
    std::shared_ptr<ClientInventory> getPersonalInventory() const;
    std::shared_ptr<ClientInventory> getSharedStash(int stashIndex) const;  // 0-2
    
    // Send item move request to server
    void requestMoveItem(InventoryType sourceInv, int sourceX, int sourceY,
                        InventoryType destInv, int destX, int destY);
    
    // Send stack split request to server
    void requestSplitStack(InventoryType invType, int x, int y, int amount, int destX, int destY);
    
    bool sendMessage(const NetworkMessage& msg);
    bool receiveMessage(NetworkMessage& msg);
    
private:
    int socket_;
    std::atomic<bool> connected_;
    std::string username_;
    std::thread listenerThread_;
    
    std::shared_ptr<ClientInventory> personalInventory_;
    std::shared_ptr<ClientInventory> sharedStashes_[3];  // 3 shared stashes (12x12 each)
    mutable std::mutex inventoryMutex_;
    
    std::vector<uint8_t> receiveBuffer_;  // Buffer for partial messages
    
    void messageListener();
    void handleInventorySync(const NetworkMessage& msg);
    void handleSharedStashSync(const NetworkMessage& msg);
};

} // namespace inventory
