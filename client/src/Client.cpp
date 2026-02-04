#include "Client.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace inventory {

Client::Client() : socket_(-1), connected_(false) {
    // Personal inventory: 12 columns x 5 rows
    personalInventory_ = std::make_shared<ClientInventory>(12, 5);
    
    // Shared stashes: 12x12 each
    for (int i = 0; i < 3; ++i) {
        sharedStashes_[i] = std::make_shared<ClientInventory>(12, 12);
    }
}

Client::~Client() {
    disconnect();
    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }
}

bool Client::connect(const char* host, int port, const std::string& username) {
    if (connected_) {
        return true;
    }
    
    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // Setup server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Connect to server
    if (::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    std::cout << "Connected to server " << host << ":" << port << std::endl;
    
    // Send login request
    NetworkMessage loginMsg(MessageType::LOGIN_REQUEST);
    loginMsg.payload.assign(username.begin(), username.end());
    
    if (!sendMessage(loginMsg)) {
        std::cerr << "Failed to send login request" << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Wait for login response
    NetworkMessage response;
    if (!receiveMessage(response)) {
        std::cerr << "Failed to receive login response" << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    if (response.type == MessageType::LOGIN_RESPONSE) {
        if (!response.payload.empty()) {
            LoginResult result = static_cast<LoginResult>(response.payload[0]);
            if (result == LoginResult::SUCCESS) {
                connected_ = true;
                username_ = username;
                std::cout << "Login successful as " << username << std::endl;
                
                // Start message listener thread (it will receive the inventory sync)
                listenerThread_ = std::thread(&Client::messageListener, this);
                
                return true;
            }
        }
    }
    else if (response.type == MessageType::LOGIN_REJECTED) {
        if (!response.payload.empty()) {
            LoginResult reason = static_cast<LoginResult>(response.payload[0]);
            switch (reason) {
                case LoginResult::USERNAME_ALREADY_CONNECTED:
                    std::cerr << "Login rejected: Username already connected" << std::endl;
                    break;
                case LoginResult::INVALID_USERNAME:
                    std::cerr << "Login rejected: Invalid username" << std::endl;
                    break;
                case LoginResult::SERVER_FULL:
                    std::cerr << "Login rejected: Server full" << std::endl;
                    break;
                default:
                    std::cerr << "Login rejected: Unknown reason" << std::endl;
            }
        }
    }
    
    close(socket_);
    socket_ = -1;
    return false;
}

void Client::disconnect() {
    if (!connected_) {
        return;
    }
    
    connected_ = false;
    
    // Send disconnect message
    NetworkMessage msg(MessageType::DISCONNECT);
    sendMessage(msg);
    
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    
    // Wait for listener thread to finish
    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }
    
    std::cout << "Disconnected from server" << std::endl;
}

bool Client::isConnected() const {
    return connected_;
}

std::shared_ptr<ClientInventory> Client::getPersonalInventory() const {
    return personalInventory_;
}

std::shared_ptr<ClientInventory> Client::getSharedStash(int stashIndex) const {
    if (stashIndex < 0 || stashIndex > 2) {
        return nullptr;
    }
    return sharedStashes_[stashIndex];
}

void Client::requestMoveItem(InventoryType sourceInv, int sourceX, int sourceY,
                             InventoryType destInv, int destX, int destY) {
    if (!connected_) {
        std::cerr << "Cannot send move request: not connected" << std::endl;
        return;
    }
    
    NetworkMessage msg(MessageType::MOVE_ITEM_REQUEST);
    
    // Payload format: [sourceInv:1][sourceX:1][sourceY:1][destInv:1][destX:1][destY:1]
    msg.payload.push_back(static_cast<uint8_t>(sourceInv));
    msg.payload.push_back(static_cast<uint8_t>(sourceX));
    msg.payload.push_back(static_cast<uint8_t>(sourceY));
    msg.payload.push_back(static_cast<uint8_t>(destInv));
    msg.payload.push_back(static_cast<uint8_t>(destX));
    msg.payload.push_back(static_cast<uint8_t>(destY));
    
    if (!sendMessage(msg)) {
        std::cerr << "Failed to send move item request" << std::endl;
    } else {
        std::cout << "Sent move request: (" << sourceX << "," << sourceY << ") -> (" 
                  << destX << "," << destY << ")" << std::endl;
    }
}

void Client::requestSplitStack(InventoryType invType, int x, int y, int amount, int destX, int destY) {
    if (!connected_) {
        std::cerr << "Cannot send split stack request: not connected" << std::endl;
        return;
    }
    
    NetworkMessage msg(MessageType::SPLIT_STACK_REQUEST);
    
    // Payload format: [invType:1][sourceX:1][sourceY:1][amount:4][destX:1][destY:1]
    msg.payload.push_back(static_cast<uint8_t>(invType));
    msg.payload.push_back(static_cast<uint8_t>(x));
    msg.payload.push_back(static_cast<uint8_t>(y));
    
    // Amount as 4 bytes (uint32_t)
    msg.payload.push_back((amount >> 24) & 0xFF);
    msg.payload.push_back((amount >> 16) & 0xFF);
    msg.payload.push_back((amount >> 8) & 0xFF);
    msg.payload.push_back(amount & 0xFF);
    
    msg.payload.push_back(static_cast<uint8_t>(destX));
    msg.payload.push_back(static_cast<uint8_t>(destY));
    
    if (!sendMessage(msg)) {
        std::cerr << "Failed to send split stack request" << std::endl;
    } else {
        std::cout << "Sent split stack request: (" << x << "," << y << ") amount=" << amount 
                  << " -> (" << destX << "," << destY << ")" << std::endl;
    }
}

bool Client::sendMessage(const NetworkMessage& msg) {
    if (socket_ < 0) {
        return false;
    }
    
    std::vector<uint8_t> data = msg.serialize();
    int bytesSent = send(socket_, data.data(), data.size(), 0);
    return bytesSent == static_cast<int>(data.size());
}

bool Client::receiveMessage(NetworkMessage& msg) {
    if (socket_ < 0) {
        return false;
    }
    
    char buffer[4096];
    int bytesRead = recv(socket_, buffer, sizeof(buffer), 0);
    
    if (bytesRead <= 0) {
        return false;
    }
    
    std::vector<uint8_t> data(buffer, buffer + bytesRead);
    msg = NetworkMessage::deserialize(data);
    
    // store the remaining data in a buffer
    if (data.size() >= 5) {
        uint32_t payloadSize = (static_cast<uint32_t>(data[1]) << 24) |
                              (static_cast<uint32_t>(data[2]) << 16) |
                              (static_cast<uint32_t>(data[3]) << 8) |
                              static_cast<uint32_t>(data[4]);
        size_t firstMessageSize = 5 + payloadSize;
        
        if (data.size() > firstMessageSize) {
            // more data after the first message - happens when the server sends 2 messages too quickly
            receiveBuffer_.insert(receiveBuffer_.end(), 
                                 data.begin() + firstMessageSize, 
                                 data.end());
            std::cout << "Saved " << (data.size() - firstMessageSize) 
                      << " extra bytes for listener thread" << std::endl;
        }
    }
    
    return true;
}

void Client::messageListener() {
    std::cout << "Message listener thread started" << std::endl;
    while (connected_ && socket_ >= 0) {
        // checking if there are already complete messages in the buffer
        bool hasDataToProcess = receiveBuffer_.size() >= 5;
        
        // If buffer is empty or incomplete, keep reading
        if (!hasDataToProcess) {
            char buffer[4096];
            int bytesRead = recv(socket_, buffer, sizeof(buffer), 0);
            
            if (bytesRead <= 0) {
                // Connection lost
                if (connected_) {
                    std::cout << "\nConnection to server lost." << std::endl;
                    connected_ = false;
                    break;
                }
                continue;
            }
            
            std::cout << "Received " << bytesRead << " bytes from server" << std::endl;
            
            // append to the buffer
            receiveBuffer_.insert(receiveBuffer_.end(), buffer, buffer + bytesRead);
        }
        
        // process all complete messages in buffer
        while (receiveBuffer_.size() >= 5) {
            uint32_t payloadSize = (static_cast<uint32_t>(receiveBuffer_[1]) << 24) |
                                  (static_cast<uint32_t>(receiveBuffer_[2]) << 16) |
                                  (static_cast<uint32_t>(receiveBuffer_[3]) << 8) |
                                  static_cast<uint32_t>(receiveBuffer_[4]);
            
            size_t totalMessageSize = 5 + payloadSize;
            
            // checking if we have the complete message
            if (receiveBuffer_.size() < totalMessageSize) {
                break; // wait for more data
            }
            
            // deserialize the message
            std::vector<uint8_t> messageData(receiveBuffer_.begin(), 
                                            receiveBuffer_.begin() + totalMessageSize);
            NetworkMessage msg = NetworkMessage::deserialize(messageData);
            
            std::cout << "Processing message type: " << static_cast<int>(msg.type) << std::endl;
            
            // delete read message from buffer
            receiveBuffer_.erase(receiveBuffer_.begin(), 
                                receiveBuffer_.begin() + totalMessageSize);
            
            // handle the message
            if (msg.type == MessageType::SERVER_SHUTDOWN) {
                std::cout << "\nServer is shutting down. Disconnecting..." << std::endl;
                connected_ = false;
                return;
            }
            else if (msg.type == MessageType::INVENTORY_FULL_SYNC) {
                handleInventorySync(msg);
            }
            else if (msg.type == MessageType::OPERATION_RESULT) {
                if (msg.payload.size() >= 2) {
                    uint8_t resultCode = msg.payload[0];
                    uint8_t messageLen = msg.payload[1];
                    
                    std::string resultMsg;
                    if (msg.payload.size() >= 2 + static_cast<size_t>(messageLen)) {
                        resultMsg = std::string(msg.payload.begin() + 2, 
                                               msg.payload.begin() + 2 + messageLen);
                    }
                    
                    if (resultCode == 0) {
                        std::cout << "Operation successful: " << resultMsg << std::endl;
                    } else {
                        std::cout << "Operation failed: " << resultMsg << std::endl;
                    }
                }
            }
            else if (msg.type == MessageType::SHARED_STASH_UPDATE) {
                handleSharedStashSync(msg);
            }
        }
        
        // trying to avoid busy waitin
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Client::handleInventorySync(const NetworkMessage& msg) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    if (personalInventory_->updateFromSyncData(msg.payload)) {
        std::cout << "Inventory synced from server" << std::endl;
    } else {
        std::cerr << "Failed to parse inventory sync" << std::endl;
    }
}

void Client::handleSharedStashSync(const NetworkMessage& msg) {
    // Payload format: [stashIndex:1byte][inventoryData...]
    if (msg.payload.empty()) {
        std::cerr << "Empty shared stash sync payload" << std::endl;
        return;
    }
    
    uint8_t stashIndex = msg.payload[0];
    if (stashIndex > 2) {
        std::cerr << "Invalid stash index: " << (int)stashIndex << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::vector<uint8_t> inventoryData(msg.payload.begin() + 1, msg.payload.end());
    if (sharedStashes_[stashIndex]->updateFromSyncData(inventoryData)) {
        std::cout << "Shared stash " << (int)stashIndex << " synced from server" << std::endl;
    } else {
        std::cerr << "Failed to parse shared stash sync" << std::endl;
    }
}

} // namespace inventory
