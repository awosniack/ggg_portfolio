#include "InventoryManager.hpp"
#include <iostream>

namespace inventory {

InventoryManager::InventoryManager() {
    sharedStashManager_ = std::make_unique<SharedStashManager>();
}

InventoryManager::~InventoryManager() = default;

Inventory* InventoryManager::getOrCreatePersonalInventory(const std::string& username) {
    std::lock_guard<std::mutex> lock(inventoriesMutex_);
    
    auto it = personalInventories_.find(username);
    if (it != personalInventories_.end()) {
        return it->second.get();
    }
    
    // new personal inventory: 12 wide x 5 tall
    auto inventory = std::make_unique<Inventory>(12, 5);
    Inventory* ptr = inventory.get();
    personalInventories_[username] = std::move(inventory);
    
    std::cout << "Created personal inventory for " << username << " (12 wide x 5 tall)" << std::endl;
    return ptr;
}

Inventory* InventoryManager::getPersonalInventory(const std::string& username) {
    std::lock_guard<std::mutex> lock(inventoriesMutex_);
    
    auto it = personalInventories_.find(username);
    if (it != personalInventories_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

void InventoryManager::removePersonalInventory(const std::string& username) {
    std::lock_guard<std::mutex> lock(inventoriesMutex_);
    personalInventories_.erase(username);
}

std::shared_ptr<Inventory> InventoryManager::getSharedStash(int stashIndex) {
    if (stashIndex < 0 || stashIndex > 2) {
        return nullptr;
    }
    return sharedStashManager_->getSharedStash(stashIndex);
}

InventoryManager::OperationResult InventoryManager::moveItem(
    Inventory* sourceInv,
    const GridPosition& sourcePos,
    Inventory* destInv,
    const GridPosition& destPos
) {
    if (!sourceInv || !destInv) {
        return OperationResult::INVALID_SOURCE;
    }
    
    // item source position
    const InventorySlot* sourceSlot = sourceInv->getSlot(sourcePos);
    if (!sourceSlot || !sourceSlot->item) {
        return OperationResult::ITEM_NOT_FOUND;
    }
    
    std::shared_ptr<Item> item = sourceSlot->item;
    uint32_t stackCount = sourceSlot->stackCount;
    ItemSize itemSize = item->getSize();
    
    // check if source and destination overlap
    bool sameInventory = (sourceInv == destInv);
    bool overlaps = false;
    if (sameInventory) {
        int sourceEndX = sourcePos.x + itemSize.width - 1;
        int sourceEndY = sourcePos.y + itemSize.height - 1;
        int destEndX = destPos.x + itemSize.width - 1;
        int destEndY = destPos.y + itemSize.height - 1;
        
        overlaps = !(destEndX < sourcePos.x || destPos.x > sourceEndX ||
                    destEndY < sourcePos.y || destPos.y > sourceEndY);
    }
    
    // if moving within same inventory and positions overlap, remove source first 
    // fix for when you select a bigger than 1x1 item and try to move it where the new position would overlap with the initial position
    std::optional<InventorySlot> tempRemoved;
    if (overlaps) {
        tempRemoved = sourceInv->removeItem(sourcePos);
        if (!tempRemoved) {
            return OperationResult::CONCURRENT_MODIFICATION;
        }
    }
    
    // check if destination can fit the item
    if (!destInv->canPlaceItem(*item, destPos)) {
        // restore the temporarily removed item if needed
        if (tempRemoved) {
            sourceInv->placeItem(item, stackCount, sourcePos);
        }
        
        // merge if destination has same item
        const InventorySlot* destSlot = destInv->getSlot(destPos);
        if (destSlot && destSlot->item && 
            destSlot->item->getId() == item->getId() &&
            destSlot->stackCount < item->getStackLimit()) {
            
            // merge stacks - need to remove and re-add with new count
            int availableSpace = item->getStackLimit() - destSlot->stackCount;
            int amountToMove = std::min(static_cast<int>(stackCount), availableSpace);
            
            // remove source
            auto removed = sourceInv->removeItem(sourcePos);
            if (!removed) {
                return OperationResult::CONCURRENT_MODIFICATION;
            }
            
            // remove destination
            auto destRemoved = destInv->removeItem(destPos);
            if (!destRemoved) {
                // rollback to put the source abck
                sourceInv->placeItem(item, stackCount, sourcePos);
                return OperationResult::CONCURRENT_MODIFICATION;
            }
            
            // place merged stack at destination
            uint32_t newDestCount = destRemoved->stackCount + amountToMove;
            if (!destInv->placeItem(item, newDestCount, destPos)) {
                // rollback both
                sourceInv->placeItem(item, stackCount, sourcePos);
                destInv->placeItem(item, destRemoved->stackCount, destPos);
                return OperationResult::NO_SPACE;
            }
            
            // if not all moved, put remainder back at source
            uint32_t remaining = stackCount - amountToMove;
            if (remaining > 0) {
                sourceInv->placeItem(item, remaining, sourcePos);
            }
            
            std::cout << "Merged " << amountToMove << " of " << item->getName() << std::endl;
            return OperationResult::SUCCESS;
        }
        
        return OperationResult::NO_SPACE;
    }
    
    // remove from source (if not already removed due to overlap)
    if (!tempRemoved) {
        tempRemoved = sourceInv->removeItem(sourcePos);
        if (!tempRemoved) {
            return OperationResult::CONCURRENT_MODIFICATION;
        }
    }
    
    // place at destination
    if (!destInv->placeItem(item, stackCount, destPos)) {
        // rollback: put it back in source
        sourceInv->placeItem(item, stackCount, sourcePos);
        return OperationResult::NO_SPACE;
    }
    
    std::cout << "Moved " << item->getName() << " (" << stackCount << ") from " 
              << sourcePos.x << "," << sourcePos.y << " to " 
              << destPos.x << "," << destPos.y << std::endl;
    
    return OperationResult::SUCCESS;
}

InventoryManager::OperationResult InventoryManager::splitStack(
    Inventory* inventory,
    const GridPosition& pos,
    int amount,
    const GridPosition& destPos
) {
    if (!inventory) {
        return OperationResult::INVALID_SOURCE;
    }
    
    if (amount <= 0) {
        return OperationResult::INVALID_STACK_SIZE;
    }
    
    // get source slot
    const InventorySlot* sourceSlot = inventory->getSlot(pos);
    if (!sourceSlot || !sourceSlot->item) {
        return OperationResult::ITEM_NOT_FOUND;
    }
    
    if (static_cast<int>(sourceSlot->stackCount) <= amount) {
        return OperationResult::INVALID_STACK_SIZE;
    }
    
    std::shared_ptr<Item> item = sourceSlot->item;
    uint32_t originalCount = sourceSlot->stackCount;
    
    // check if destination can fit the split stack
    if (!inventory->canPlaceItem(*item, destPos)) {
        return OperationResult::NO_SPACE;
    }
    
    // remove original stack
    auto removed = inventory->removeItem(pos);
    if (!removed) {
        return OperationResult::CONCURRENT_MODIFICATION;
    }
    
    // place split amount at destination
    if (!inventory->placeItem(item, amount, destPos)) {
        // rollback
        inventory->placeItem(item, originalCount, pos);
        return OperationResult::NO_SPACE;
    }
    
    // place remaining at source
    uint32_t remaining = originalCount - amount;
    if (!inventory->placeItem(item, remaining, pos)) {
        // rollback: remove dest, restore source
        inventory->removeItem(destPos);
        inventory->placeItem(item, originalCount, pos);
        return OperationResult::NO_SPACE;
    }
    
    std::cout << "Split " << amount << " of " << item->getName() 
              << " from stack of " << originalCount << std::endl;
    
    return OperationResult::SUCCESS;
}

} // namespace inventory
