#pragma once

#include "Item.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace inventory {

// registry of all available items
class ItemRegistry {
public:
    static ItemRegistry& getInstance();
    
    void initialize();
    
    // get item by id
    std::shared_ptr<Item> getItem(uint32_t id) const;
    
    // get all items
    std::vector<std::shared_ptr<Item>> getAllItems() const;
    
private:
    ItemRegistry() = default;
    std::unordered_map<uint32_t, std::shared_ptr<Item>> items_;
    
    void registerItem(std::shared_ptr<Item> item);
};

} // namespace inventory
