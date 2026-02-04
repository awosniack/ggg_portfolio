#include "Server.hpp"
#include "ClientSession.hpp"
#include "InventoryManager.hpp"
#include "ItemRegistry.hpp"
#include "NetworkMessage.hpp"
#include <iostream>
#include <cstring>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

namespace inventory
{

    class ServerImpl
    {
    public:
        int serverSocket = -1;
        std::map<int, std::unique_ptr<ClientSession>> clients; // socket -> session
        std::map<std::string, int> usernameToSocket;           // username -> socket (for active connections)
        std::mutex clientsMutex;
        std::unique_ptr<InventoryManager> inventoryManager;

        ServerImpl()
        {
            inventoryManager = std::make_unique<InventoryManager>();
        }

        void acceptClient(int serverSocket);
        void handleClient(int clientSocket);
        std::string receiveMessage(int socket);
        bool sendMessage(int socket, const NetworkMessage &msg);
        void disconnectClient(int clientSocket);
        void disconnectClientNoLock(int clientSocket); // version without lock - used when same username as a already online user tries to join the server

        // handlers
        void handleMoveItemRequest(int clientSocket, const NetworkMessage &msg);
        void handleSplitStackRequest(int clientSocket, const NetworkMessage &msg);

        // helper to serialize inventory for sync
        std::vector<uint8_t> serializeInventory(const Inventory *inventory);
    };

    Server::Server(int port) : port_(port), running_(false)
    {
        impl_ = std::make_unique<ServerImpl>();
    }

    Server::~Server()
    {
        stop();
    }

    void Server::start()
    {
        if (running_)
        {
            return;
        }

        running_ = true;
        serverThread_ = std::thread(&Server::run, this);
        std::cout << "Server starting on port " << port_ << std::endl;
    }

    void Server::stop()
    {
        if (!running_)
        {
            return;
        }

        running_ = false;

        // shutdown - notify the clients
        {
            std::lock_guard<std::mutex> lock(impl_->clientsMutex);
            NetworkMessage shutdownMsg;
            shutdownMsg.type = MessageType::SERVER_SHUTDOWN;

            for (auto &[socket, session] : impl_->clients)
            {
                impl_->sendMessage(socket, shutdownMsg);
            }
        }

        // wait some time so clients receive shutdown message
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // closing server socket
        if (impl_->serverSocket >= 0)
        {
            shutdown(impl_->serverSocket, SHUT_RDWR);
            close(impl_->serverSocket);
            impl_->serverSocket = -1;
        }

        // close client sockets
        {
            std::lock_guard<std::mutex> lock(impl_->clientsMutex);
            for (auto &[socket, session] : impl_->clients)
            {
                shutdown(socket, SHUT_RDWR);
                close(socket);
            }
            impl_->clients.clear();
        }

        if (serverThread_.joinable())
        {
            serverThread_.join();
        }

        std::cout << "Server stopped" << std::endl;
    }

    bool Server::isRunning() const
    {
        return running_;
    }

    std::vector<std::string> Server::getConnectedPlayers() const
    {
        std::lock_guard<std::mutex> lock(impl_->clientsMutex);
        std::vector<std::string> players;
        for (const auto &[username, socket] : impl_->usernameToSocket)
        {
            players.push_back(username);
        }
        return players;
    }

    bool Server::giveItem(const std::string &username, uint32_t itemId, uint32_t count)
    {
        // get the item from registry
        auto &registry = ItemRegistry::getInstance();
        auto item = registry.getItem(itemId);
        if (!item)
        {
            std::cerr << "Item " << itemId << " not found in registry" << std::endl;
            return false;
        }

        // get player inventory
        Inventory *inventory = impl_->inventoryManager->getPersonalInventory(username);
        if (!inventory)
        {
            std::cerr << "Player " << username << " not found or not logged in" << std::endl;
            return false;
        }

        // try to place item in first available slot
        for (int y = 0; y < inventory->getHeight(); ++y)
        {
            for (int x = 0; x < inventory->getWidth(); ++x)
            {
                GridPosition pos(x, y);
                if (inventory->canPlaceItem(*item, pos))
                {
                    if (inventory->placeItem(item, count, pos))
                    {
                        std::cout << "Gave " << count << "x " << item->getName()
                                  << " to " << username << " at (" << x << "," << y << ")" << std::endl;

                        // send inventory update to client
                        std::lock_guard<std::mutex> lock(impl_->clientsMutex);
                        auto socketIt = impl_->usernameToSocket.find(username);
                        if (socketIt != impl_->usernameToSocket.end())
                        {
                            NetworkMessage sync(MessageType::INVENTORY_FULL_SYNC);
                            sync.payload = impl_->serializeInventory(inventory);
                            impl_->sendMessage(socketIt->second, sync);
                        }

                        return true;
                    }
                }
            }
        }

        std::cerr << "No space in " << username << "'s inventory for " << item->getName() << std::endl;
        return false;
    }

    Inventory *Server::getPlayerInventory(const std::string &username)
    {
        return impl_->inventoryManager->getPersonalInventory(username);
    }

    void Server::run()
    {
        // create socket
        impl_->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (impl_->serverSocket < 0)
        {
            std::cerr << "Failed to create socket" << std::endl;
            running_ = false;
            return;
        }

        // set the socket to reuse address
        int opt = 1;
        setsockopt(impl_->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // non-blocking socket
        fcntl(impl_->serverSocket, F_SETFL, O_NONBLOCK);

        // binding the socket
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);

        if (bind(impl_->serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            std::cerr << "Failed to bind socket on port " << port_ << std::endl;
            close(impl_->serverSocket);
            running_ = false;
            return;
        }

        // listen for incoming connections
        if (listen(impl_->serverSocket, 10) < 0)
        {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(impl_->serverSocket);
            running_ = false;
            return;
        }

        std::cout << "Server listening on port " << port_ << std::endl;

        while (running_)
        {
            // accept new connections
            impl_->acceptClient(impl_->serverSocket);

            // handle existing clients
            std::vector<int> socketsToHandle;

            {
                std::lock_guard<std::mutex> lock(impl_->clientsMutex);
                for (auto &[socket, session] : impl_->clients)
                {
                    socketsToHandle.push_back(socket);
                }
            }

            // handle clients outside the lock
            for (int socket : socketsToHandle)
            {
                impl_->handleClient(socket);
            }
            // avoid busy waiting
            usleep(10000); // 10ms
        }

        // cleanup
        std::lock_guard<std::mutex> lock(impl_->clientsMutex);
        impl_->clients.clear();
    }

    void ServerImpl::acceptClient(int serverSocket)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &clientLen);
        if (clientSocket < 0)
        {
            // EWOULDBLOCK or EAGAIN is expected for non-blocking sockets
            // other errors during shutdown are also to be expected
            return;
        }

        // set client socket to non-blocking
        fcntl(clientSocket, F_SETFL, O_NONBLOCK);

        std::lock_guard<std::mutex> lock(clientsMutex);
        clients[clientSocket] = std::make_unique<ClientSession>(clientSocket);

        std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr)
                  << " (socket: " << clientSocket << ")" << std::endl;
    }

    void ServerImpl::handleClient(int clientSocket)
    {
        // validate if client still exists
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            if (clients.find(clientSocket) == clients.end())
            {
                return;
            }
        }

        std::string data = receiveMessage(clientSocket);
        if (data.empty())
        {
            return;
        }

        // parse the message
        NetworkMessage msg = NetworkMessage::deserialize(std::vector<uint8_t>(data.begin(), data.end()));

        if (msg.type == MessageType::LOGIN_REQUEST)
        {
            std::lock_guard<std::mutex> lock(clientsMutex);

            // get username from payload
            std::string username(msg.payload.begin(), msg.payload.end());

            std::cout << "Login request from socket " << clientSocket << " with username: " << username << std::endl;

            NetworkMessage response;

            // validate the username
            if (username.empty() || username.length() > 32)
            {
                response.type = MessageType::LOGIN_REJECTED;
                response.payload.push_back(static_cast<uint8_t>(LoginResult::INVALID_USERNAME));
                sendMessage(clientSocket, response);
                disconnectClientNoLock(clientSocket);
                return;
            }

            // is user already connected
            bool alreadyConnected = false;
            for (const auto &[sock, session] : clients)
            {
                if (session->getUsername() == username && sock != clientSocket)
                {
                    alreadyConnected = true;
                    break;
                }
            }

            if (alreadyConnected)
            {
                std::cout << "Username " << username << " already connected, rejecting" << std::endl;
                response.type = MessageType::LOGIN_REJECTED;
                response.payload.push_back(static_cast<uint8_t>(LoginResult::USERNAME_ALREADY_CONNECTED));
                sendMessage(clientSocket, response);
                disconnectClientNoLock(clientSocket);
                return;
            }

            // accept the login
            clients[clientSocket]->setUsername(username);
            usernameToSocket[username] = clientSocket;

            // get or create persistent inventory through InventoryManager
            Inventory *inventory = inventoryManager->getOrCreatePersonalInventory(username);
            (void)inventory;

            std::cout << "Login accepted for " << username << std::endl;

            response.type = MessageType::LOGIN_RESPONSE;
            response.payload.push_back(static_cast<uint8_t>(LoginResult::SUCCESS));
            sendMessage(clientSocket, response);

            // send inventory sync
            NetworkMessage inventorySync;
            inventorySync.type = MessageType::INVENTORY_FULL_SYNC;
            inventorySync.payload = serializeInventory(inventory);
            sendMessage(clientSocket, inventorySync);

            // send all 3 shared stash syncs
            for (int i = 0; i < 3; ++i)
            {
                auto stash = inventoryManager->getSharedStash(i);
                if (stash)
                {
                    NetworkMessage stashSync(MessageType::SHARED_STASH_UPDATE);
                    stashSync.payload.push_back(static_cast<uint8_t>(i)); // stash index
                    auto stashData = serializeInventory(stash.get());
                    stashSync.payload.insert(stashSync.payload.end(), stashData.begin(), stashData.end());
                    sendMessage(clientSocket, stashSync);
                }
            }

            std::cout << "Sent inventory sync to " << username << std::endl;
        }
        else if (msg.type == MessageType::DISCONNECT)
        {
            disconnectClient(clientSocket);
        }
        else if (msg.type == MessageType::HEARTBEAT)
        {
            NetworkMessage response(MessageType::HEARTBEAT);
            sendMessage(clientSocket, response);
        }
        else if (msg.type == MessageType::MOVE_ITEM_REQUEST)
        {
            handleMoveItemRequest(clientSocket, msg);
        }
        else if (msg.type == MessageType::SPLIT_STACK_REQUEST)
        {
            handleSplitStackRequest(clientSocket, msg);
        }
        else if (msg.type == MessageType::SPLIT_STACK_REQUEST)
        {
            handleSplitStackRequest(clientSocket, msg);
        }
    }

    std::string ServerImpl::receiveMessage(int socket)
    {
        char buffer[4096];
        int bytesRead = recv(socket, buffer, sizeof(buffer), 0);

        if (bytesRead > 0)
        {
            return std::string(buffer, bytesRead);
        }
        else if (bytesRead == 0)
        {
            disconnectClient(socket);
        }

        return "";
    }

    bool ServerImpl::sendMessage(int socket, const NetworkMessage &msg)
    {
        std::vector<uint8_t> data = msg.serialize();
        int bytesSent = send(socket, data.data(), data.size(), 0);
        return bytesSent == static_cast<int>(data.size());
    }

    void ServerImpl::disconnectClient(int clientSocket)
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        disconnectClientNoLock(clientSocket);
    }

    void ServerImpl::disconnectClientNoLock(int clientSocket)
    {
        // assuming the caller already holds clientsMutex
        std::string username;

        auto it = clients.find(clientSocket);
        if (it != clients.end())
        {
            username = it->second->getUsername();
            clients.erase(it);
        }

        if (!username.empty())
        {
            usernameToSocket.erase(username);
            std::cout << "Client " << username << " disconnected" << std::endl;
        }
        else
        {
            std::cout << "Client (socket " << clientSocket << ") disconnected before login" << std::endl;
        }

        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
    }

    void ServerImpl::handleMoveItemRequest(int clientSocket, const NetworkMessage &msg)
    {
        // Payload format: [sourceInvType:1byte][sourceX:1byte][sourceY:1byte]
        //                 [destInvType:1byte][destX:1byte][destY:1byte]
        // InvType: 0=personal, 1-3=shared stash 0-2

        if (msg.payload.size() < 6)
        {
            std::cerr << "Invalid MOVE_ITEM_REQUEST payload size" << std::endl;
            return;
        }

        std::string username = clients[clientSocket]->getUsername();
        if (username.empty())
        {
            return;
        }

        uint8_t sourceInvType = msg.payload[0];
        GridPosition sourcePos(msg.payload[1], msg.payload[2]);
        uint8_t destInvType = msg.payload[3];
        GridPosition destPos(msg.payload[4], msg.payload[5]);

        // source inventory
        Inventory *sourceInv = nullptr;
        if (sourceInvType == 0)
        {
            sourceInv = inventoryManager->getPersonalInventory(username);
        }
        else if (sourceInvType >= 1 && sourceInvType <= 3)
        {
            auto sharedStash = inventoryManager->getSharedStash(sourceInvType - 1);
            sourceInv = sharedStash.get();
        }

        // destination inventory
        Inventory *destInv = nullptr;
        if (destInvType == 0)
        {
            destInv = inventoryManager->getPersonalInventory(username);
        }
        else if (destInvType >= 1 && destInvType <= 3)
        {
            auto sharedStash = inventoryManager->getSharedStash(destInvType - 1);
            destInv = sharedStash.get();
        }

        // move
        auto result = inventoryManager->moveItem(sourceInv, sourcePos, destInv, destPos);

        // send result
        NetworkMessage response(MessageType::OPERATION_RESULT);
        response.payload.push_back(static_cast<uint8_t>(result));
        sendMessage(clientSocket, response);

        // when successful, send inventory update
        if (result == InventoryManager::OperationResult::SUCCESS)
        {
            // send personal inventory sync if it was involved
            if (sourceInvType == 0 || destInvType == 0)
            {
                NetworkMessage sync(MessageType::INVENTORY_FULL_SYNC);
                sync.payload = serializeInventory(inventoryManager->getPersonalInventory(username));
                sendMessage(clientSocket, sync);
            }

            // broadcast shared stash update to ALL clients
            if (sourceInvType >= 1 && sourceInvType <= 3)
            {
                int stashIndex = sourceInvType - 1;
                auto stash = inventoryManager->getSharedStash(stashIndex);
                NetworkMessage stashSync(MessageType::SHARED_STASH_UPDATE);
                stashSync.payload.push_back(static_cast<uint8_t>(stashIndex));
                auto stashData = serializeInventory(stash.get());
                stashSync.payload.insert(stashSync.payload.end(), stashData.begin(), stashData.end());

                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto &[sock, session] : clients)
                {
                    sendMessage(sock, stashSync);
                }
            }
            if (destInvType >= 1 && destInvType <= 3 && destInvType != sourceInvType)
            {
                int stashIndex = destInvType - 1;
                auto stash = inventoryManager->getSharedStash(stashIndex);
                NetworkMessage stashSync(MessageType::SHARED_STASH_UPDATE);
                stashSync.payload.push_back(static_cast<uint8_t>(stashIndex));
                auto stashData = serializeInventory(stash.get());
                stashSync.payload.insert(stashSync.payload.end(), stashData.begin(), stashData.end());

                // broadcast to all connected clients
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto &[sock, session] : clients)
                {
                    sendMessage(sock, stashSync);
                }
            }
        }
    }

    void ServerImpl::handleSplitStackRequest(int clientSocket, const NetworkMessage &msg)
    {
        // Payload format: [invType:1byte][sourceX:1byte][sourceY:1byte]
        //                 [amount:4bytes][destX:1byte][destY:1byte]

        if (msg.payload.size() < 8)
        {
            std::cerr << "Invalid SPLIT_STACK_REQUEST payload size" << std::endl;
            return;
        }

        std::string username = clients[clientSocket]->getUsername();
        if (username.empty())
        {
            return;
        }

        uint8_t invType = msg.payload[0];
        GridPosition sourcePos(msg.payload[1], msg.payload[2]);

        uint32_t amount = (static_cast<uint32_t>(msg.payload[3]) << 24) |
                          (static_cast<uint32_t>(msg.payload[4]) << 16) |
                          (static_cast<uint32_t>(msg.payload[5]) << 8) |
                          static_cast<uint32_t>(msg.payload[6]);

        GridPosition destPos(msg.payload[7], msg.payload[8]);

        // source inventory
        Inventory *inventory = nullptr;
        if (invType == 0)
        {
            inventory = inventoryManager->getPersonalInventory(username);
        }
        else if (invType >= 1 && invType <= 3)
        {
            auto sharedStash = inventoryManager->getSharedStash(invType - 1);
            inventory = sharedStash.get();
        }

        // split
        auto result = inventoryManager->splitStack(inventory, sourcePos, amount, destPos);

        // send result
        NetworkMessage response(MessageType::OPERATION_RESULT);
        response.payload.push_back(static_cast<uint8_t>(result));
        sendMessage(clientSocket, response);

        // when successful, send inventory update
        if (result == InventoryManager::OperationResult::SUCCESS)
        {
            if (invType == 0)
            {
                // personal inventory
                NetworkMessage sync(MessageType::INVENTORY_FULL_SYNC);
                sync.payload = serializeInventory(inventoryManager->getPersonalInventory(username));
                sendMessage(clientSocket, sync);
            }
            else if (invType >= 1 && invType <= 3)  // item splitting is only being allowed inside personal inventory
            {   
                // shared stash - broadcast to all clients
                int stashIndex = invType - 1;
                auto stash = inventoryManager->getSharedStash(stashIndex);
                NetworkMessage stashSync(MessageType::SHARED_STASH_UPDATE);
                stashSync.payload.push_back(static_cast<uint8_t>(stashIndex));
                auto stashData = serializeInventory(stash.get());
                stashSync.payload.insert(stashSync.payload.end(), stashData.begin(), stashData.end());

                // broadcast to all connected clients
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto &[sock, session] : clients)
                {
                    sendMessage(sock, stashSync);
                }
            }
        }
    }

    std::vector<uint8_t> ServerImpl::serializeInventory(const Inventory *inventory)
    {
        std::vector<uint8_t> data;

        if (!inventory)
        {
            return data;
        }

        // Format: [width:1byte][height:1byte][itemCount:2bytes]
        // For each item: [x:1byte][y:1byte][itemId:4bytes][stackCount:4bytes][itemName_length:1byte][itemName:n][size_w:1byte][size_h:1byte][stackLimit:4bytes]

        data.push_back(static_cast<uint8_t>(inventory->getWidth()));
        data.push_back(static_cast<uint8_t>(inventory->getHeight()));

        auto items = inventory->getAllItems();
        uint16_t itemCount = static_cast<uint16_t>(items.size());
        data.push_back((itemCount >> 8) & 0xFF);
        data.push_back(itemCount & 0xFF);

        for (const auto &slot : items)
        {
            if (!slot.item)
                continue;

            // pos
            data.push_back(static_cast<uint8_t>(slot.position.x));
            data.push_back(static_cast<uint8_t>(slot.position.y));

            // id
            uint32_t itemId = slot.item->getId();
            data.push_back((itemId >> 24) & 0xFF);
            data.push_back((itemId >> 16) & 0xFF);
            data.push_back((itemId >> 8) & 0xFF);
            data.push_back(itemId & 0xFF);

            // stack
            uint32_t stackCount = slot.stackCount;
            data.push_back((stackCount >> 24) & 0xFF);
            data.push_back((stackCount >> 16) & 0xFF);
            data.push_back((stackCount >> 8) & 0xFF);
            data.push_back(stackCount & 0xFF);

            // name
            std::string name = slot.item->getName();
            data.push_back(static_cast<uint8_t>(name.length()));
            data.insert(data.end(), name.begin(), name.end());

            // size
            ItemSize size = slot.item->getSize();
            data.push_back(static_cast<uint8_t>(size.width));
            data.push_back(static_cast<uint8_t>(size.height));

            // limit
            uint32_t stackLimit = slot.item->getStackLimit();
            data.push_back((stackLimit >> 24) & 0xFF);
            data.push_back((stackLimit >> 16) & 0xFF);
            data.push_back((stackLimit >> 8) & 0xFF);
            data.push_back(stackLimit & 0xFF);
        }

        return data;
    }

} // namespace inventory
