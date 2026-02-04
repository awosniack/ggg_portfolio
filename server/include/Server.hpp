#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace inventory {

class ServerImpl;
class Inventory;
class Item;

class Server {
public:
    Server(int port);
    ~Server();
    
    void start();
    void stop();
    bool isRunning() const;
    
    void run();
    
    // test/admin api
    std::vector<std::string> getConnectedPlayers() const;
    bool giveItem(const std::string& username, uint32_t itemId, uint32_t count);
    Inventory* getPlayerInventory(const std::string& username);
    
private:
    int port_;
    std::atomic<bool> running_;
    std::unique_ptr<ServerImpl> impl_;
    std::thread serverThread_;
};

} // namespace inventory
