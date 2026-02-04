#include "Inventory.hpp"

namespace inventory {

Inventory::Inventory(int width, int height) 
    : width_(width), height_(height) {
    grid_.resize(height);
    for (int y = 0; y < height; ++y) {
        grid_[y].resize(width);
        for (int x = 0; x < width; ++x) {
            grid_[y][x].position = GridPosition(x, y);
        }
    }
}

bool Inventory::isPositionValid(GridPosition pos) const {
    return pos.x >= 0 && pos.x < width_ && pos.y >= 0 && pos.y < height_;
}

bool Inventory::isAreaOccupied(GridPosition pos, ItemSize size) const {
    for (int y = pos.y; y < pos.y + size.height; ++y) {
        for (int x = pos.x; x < pos.x + size.width; ++x) {
            if (!isPositionValid(GridPosition(x, y))) {
                return true; // out of bounds counts as occupied
            }
            if (!grid_[y][x].isEmpty() || grid_[y][x].isOccupied) {
                return true;
            }
        }
    }
    return false;
}

bool Inventory::canPlaceItem(const Item& item, GridPosition pos) const {
    return !isAreaOccupied(pos, item.getSize());
}

bool Inventory::placeItem(std::shared_ptr<Item> item, uint32_t count, GridPosition pos) {
    if (!item || count == 0 || count > item->getStackLimit()) {
        return false;
    }
    
    if (!canPlaceItem(*item, pos)) {
        return false;
    }
    
    occupyArea(pos, item->getSize(), item, count);
    return true;
}

void Inventory::occupyArea(GridPosition pos, ItemSize size, std::shared_ptr<Item> item, uint32_t count) {
    for (int y = pos.y; y < pos.y + size.height; ++y) {
        for (int x = pos.x; x < pos.x + size.width; ++x) {
            if (x == pos.x && y == pos.y) {
                // top-left cell stores the item
                grid_[y][x].item = item;
                grid_[y][x].stackCount = count;
                grid_[y][x].isOccupied = false;
            } else {
                // other cells are marked as occupied
                grid_[y][x].item = nullptr;
                grid_[y][x].stackCount = 0;
                grid_[y][x].isOccupied = true;
            }
        }
    }
}

void Inventory::clearArea(GridPosition pos, ItemSize size) {
    for (int y = pos.y; y < pos.y + size.height; ++y) {
        for (int x = pos.x; x < pos.x + size.width; ++x) {
            if (isPositionValid(GridPosition(x, y))) {
                grid_[y][x].item = nullptr;
                grid_[y][x].stackCount = 0;
                grid_[y][x].isOccupied = false;
            }
        }
    }
}

std::optional<InventorySlot> Inventory::removeItem(GridPosition pos) {
    if (!isPositionValid(pos)) {
        return std::nullopt;
    }
    
    InventorySlot& slot = grid_[pos.y][pos.x];
    if (slot.isEmpty()) {
        return std::nullopt;
    }
    
    InventorySlot result = slot;
    clearArea(pos, slot.item->getSize());
    
    return result;
}

const InventorySlot* Inventory::getSlot(GridPosition pos) const {
    if (!isPositionValid(pos)) {
        return nullptr;
    }
    return &grid_[pos.y][pos.x];
}

std::vector<InventorySlot> Inventory::getAllItems() const {
    std::vector<InventorySlot> items;
    
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto& slot = grid_[y][x];
            if (!slot.isEmpty()) {
                items.push_back(slot);
            }
        }
    }
    
    return items;
}

void Inventory::clear() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            grid_[y][x].item = nullptr;
            grid_[y][x].stackCount = 0;
            grid_[y][x].isOccupied = false;
        }
    }
}

} // namespace inventory
