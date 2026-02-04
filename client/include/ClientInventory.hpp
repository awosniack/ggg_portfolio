#pragma once

#include "Item.hpp"
#include "Inventory.hpp"
#include <memory>
#include <vector>

namespace inventory {

// client-side representation of inventory
// storing the items received from server INVENTORY_FULL_SYNC messages
class ClientInventory {
public:
    ClientInventory(int width, int height);
    
    void clear();
    
    // update from server data
    bool updateFromSyncData(const std::vector<uint8_t>& data);
    
    // query inventory state
    const InventorySlot* getSlot(int x, int y) const;
    std::vector<InventorySlot> getAllItems() const;
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
private:
    int width_;
    int height_;
    std::vector<InventorySlot> items_;
};

} // namespace inventory
