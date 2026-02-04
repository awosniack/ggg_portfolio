#pragma once

#include <string>
#include <chrono>

namespace inventory {

class ClientSession {
public:
    ClientSession(int socket);
    ~ClientSession();
    
    int getSocket() const { return socket_; }
    
    const std::string& getUsername() const { return username_; }
    void setUsername(const std::string& username) { 
        username_ = username;
        lastActivity_ = std::chrono::steady_clock::now();
    }
    
    bool isAuthenticated() const { return !username_.empty(); }
    
    void updateActivity() {
        lastActivity_ = std::chrono::steady_clock::now();
    }
    
    std::chrono::steady_clock::time_point getLastActivity() const {
        return lastActivity_;
    }
    
private:
    int socket_;
    std::string username_;
    std::chrono::steady_clock::time_point lastActivity_;
};

} // namespace inventory
