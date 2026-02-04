#pragma once

#include "Item.hpp"
#include <vector>
#include <memory>
#include <optional>

namespace inventory {

struct GridPosition {
    int x;
    int y;
    
    GridPosition() : x(0), y(0) {}
    GridPosition(int px, int py) : x(px), y(py) {}
    
    bool operator==(const GridPosition& other) const {
        return x == other.x && y == other.y;
    }
};

struct InventorySlot {
    std::shared_ptr<Item> item;
    uint32_t stackCount;
    GridPosition position;
    bool isOccupied;  // true when this cell is occupied by a multi-cell item (but not the origin)
    
    InventorySlot() : item(nullptr), stackCount(0), position(), isOccupied(false) {}
    
    bool isEmpty() const { return item == nullptr || stackCount == 0; }
};

class Inventory {
public:
    Inventory(int width, int height);
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    // check if item can be placed at position
    bool canPlaceItem(const Item& item, GridPosition pos) const;
    
    // place item at position (returns false if can't place)
    bool placeItem(std::shared_ptr<Item> item, uint32_t count, GridPosition pos);
    
    // remove item at position
    std::optional<InventorySlot> removeItem(GridPosition pos);
    
    // get item at position (returns the top-left slot of multi-cell items)
    const InventorySlot* getSlot(GridPosition pos) const;
    
    // get all occupied slots
    std::vector<InventorySlot> getAllItems() const;
    
    // clear inventory
    void clear();
    
private:
    int width_;
    int height_;
    std::vector<std::vector<InventorySlot>> grid_;
    
    bool isPositionValid(GridPosition pos) const;
    bool isAreaOccupied(GridPosition pos, ItemSize size) const;
    void occupyArea(GridPosition pos, ItemSize size, std::shared_ptr<Item> item, uint32_t count);
    void clearArea(GridPosition pos, ItemSize size);
};

} // namespace inventory
