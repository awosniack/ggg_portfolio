#include "Server.hpp"
#include "ItemRegistry.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <string>
#include <sstream>

std::atomic<bool> running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        running = false;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Inventory System - Server" << std::endl;
    
    // init item registry
    inventory::ItemRegistry::getInstance().initialize();
    
    // default port
    int port = 7777;
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number. Using default: 7777" << std::endl;
            port = 7777;
        }
    }
    
    inventory::Server server(port);
    server.start();
    
    std::cout << "Server running on port " << port << std::endl;
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  help          - Show this help" << std::endl;
    std::cout << "  items         - List all available items" << std::endl;
    std::cout << "  give <username> <itemId> <count> - Give item to player" << std::endl;
    std::cout << "  list          - List connected players" << std::endl;
    std::cout << "  quit          - Stop server" << std::endl;
    std::cout << "\nPress Ctrl+C or type 'quit' to stop.\n" << std::endl;
    
    // basic command IO thread
    std::thread commandThread([&]() {
        std::string line;
        while (running && server.isRunning()) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) {
                break;
            }
            
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "quit" || cmd == "exit") {
                running = false;
                break;
            }
            else if (cmd == "help") {
                std::cout << "\nAvailable commands:" << std::endl;
                std::cout << "  help          - Show this help" << std::endl;
                std::cout << "  items         - List all available items" << std::endl;
                std::cout << "  give <username> <itemId> <count> - Give item to player" << std::endl;
                std::cout << "  list          - List connected players" << std::endl;
                std::cout << "  quit          - Stop server\n" << std::endl;
            }
            else if (cmd == "items") {
                auto& registry = inventory::ItemRegistry::getInstance();
                auto items = registry.getAllItems();
                std::cout << "\nAvailable items (" << items.size() << "):" << std::endl;
                for (const auto& item : items) {
                    std::cout << "  [" << item->getId() << "] " << item->getName() 
                              << " (" << item->getSize().width << "x" << item->getSize().height 
                              << ", stack: " << item->getStackLimit() << ")" << std::endl;
                }
                std::cout << std::endl;
            }
            else if (cmd == "give") {
                std::string username;
                uint32_t itemId;
                uint32_t count;
                
                if (!(iss >> username >> itemId >> count)) {
                    std::cout << "Usage: give <username> <itemId> <count>" << std::endl;
                    continue;
                }
                
                if (server.giveItem(username, itemId, count)) {
                    std::cout << "Successfully gave item to " << username << std::endl;
                } else {
                    std::cout << "Failed to give item (check username and inventory space)" << std::endl;
                }
            }
            else if (cmd == "list") {
                auto players = server.getConnectedPlayers();
                if (players.empty()) {
                    std::cout << "No players connected" << std::endl;
                } else {
                    std::cout << "\nConnected players (" << players.size() << "):" << std::endl;
                    for (const auto& player : players) {
                        std::cout << "  - " << player << std::endl;
                    }
                    std::cout << std::endl;
                }
            }
            else {
                std::cout << "Unknown command: " << cmd << " (type 'help' for commands)" << std::endl;
            }
        }
    });
    
    // wait for shutdown signal
    while (running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // cleanup
    if (commandThread.joinable()) {
        commandThread.join();
    }
    
    server.stop();
    std::cout << "Server shutdown complete." << std::endl;
    
    server.stop();
    std::cout << "Server shutdown complete." << std::endl;
    
    return 0;
}
