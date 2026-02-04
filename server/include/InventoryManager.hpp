#pragma once

#include "Inventory.hpp"
#include "SharedStashManager.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace inventory {

// Manages all inventories in the system:
// - Personal inventories for each player (5x12)
// - 3 shared stashes (12x12 each) through SharedStashManager
class InventoryManager {
public:
    InventoryManager();
    ~InventoryManager();
    
    // Personal inventory management
    Inventory* getOrCreatePersonalInventory(const std::string& username);
    Inventory* getPersonalInventory(const std::string& username);
    void removePersonalInventory(const std::string& username);
    
    // Shared stash access (0, 1, or 2)
    std::shared_ptr<Inventory> getSharedStash(int stashIndex);
    
    // Item operations
    enum class OperationResult {
        SUCCESS,
        INVALID_SOURCE,
        INVALID_DESTINATION,
        ITEM_NOT_FOUND,
        NO_SPACE,
        INVALID_STACK_SIZE,
        CONCURRENT_MODIFICATION
    };
    
    // Move item within same inventory or between inventories
    OperationResult moveItem(
        Inventory* sourceInv,
        const GridPosition& sourcePos,
        Inventory* destInv,
        const GridPosition& destPos
    );
    
    // Split item stack
    OperationResult splitStack(
        Inventory* inventory,
        const GridPosition& pos,
        int amount,
        const GridPosition& destPos
    );
    
private:
    std::unordered_map<std::string, std::unique_ptr<Inventory>> personalInventories_;
    std::mutex inventoriesMutex_;
    
    std::unique_ptr<SharedStashManager> sharedStashManager_;
};

} // namespace inventory
