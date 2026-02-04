#include "ClientInventory.hpp"
#include <iostream>
#include <cstring>

namespace inventory {

ClientInventory::ClientInventory(int width, int height) 
    : width_(width), height_(height) {
    std::cout << "ClientInventory created: " << width_ << "x" << height_ << std::endl;
}

void ClientInventory::clear() {
    items_.clear();
}

bool ClientInventory::updateFromSyncData(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        std::cerr << "Invalid sync data: too small" << std::endl;
        return false;
    }
    
    // parse: [width:1byte][height:1byte][itemCount:2bytes]
    uint8_t width = data[0];
    uint8_t height = data[1];
    uint16_t itemCount = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    
    if (width != width_ || height != height_) {
        std::cerr << "Warning: inventory size mismatch (expected " 
                  << width_ << "x" << height_ << ", got " 
                  << (int)width << "x" << (int)height << ")" << std::endl;
    }
    
    items_.clear();
    
    size_t offset = 4;
    
    for (uint16_t i = 0; i < itemCount; ++i) {
        if (offset + 2 > data.size()) {
            std::cerr << "Truncated item data at item " << i << std::endl;
            return false;
        }
        
        uint8_t x = data[offset++];
        uint8_t y = data[offset++];
        
        if (offset + 4 > data.size()) return false;
        uint32_t itemId = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        
        if (offset + 4 > data.size()) return false;
        uint32_t stackCount = (static_cast<uint32_t>(data[offset]) << 24) |
                             (static_cast<uint32_t>(data[offset + 1]) << 16) |
                             (static_cast<uint32_t>(data[offset + 2]) << 8) |
                             static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        
        if (offset + 1 > data.size()) return false;
        uint8_t nameLen = data[offset++];
        
        if (offset + nameLen > data.size()) return false;
        std::string name(data.begin() + offset, data.begin() + offset + nameLen);
        offset += nameLen;
        
        if (offset + 2 > data.size()) return false;
        uint8_t sizeW = data[offset++];
        uint8_t sizeH = data[offset++];
        
        if (offset + 4 > data.size()) return false;
        uint32_t stackLimit = (static_cast<uint32_t>(data[offset]) << 24) |
                             (static_cast<uint32_t>(data[offset + 1]) << 16) |
                             (static_cast<uint32_t>(data[offset + 2]) << 8) |
                             static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        
        auto item = std::make_shared<Item>(itemId, name, ItemSize{sizeW, sizeH}, stackLimit, "");
        
        InventorySlot slot;
        slot.item = item;
        slot.stackCount = stackCount;
        slot.position = GridPosition(x, y);
        
        items_.push_back(slot);
    }
    
    std::cout << "Updated inventory: " << items_.size() << " items" << std::endl;
    return true;
}

const InventorySlot* ClientInventory::getSlot(int x, int y) const {
    for (const auto& slot : items_) {
        if (slot.position.x == x && slot.position.y == y) {
            return &slot;
        }
    }
    return nullptr;
}

std::vector<InventorySlot> ClientInventory::getAllItems() const {
    return items_;
}

} // namespace inventory
