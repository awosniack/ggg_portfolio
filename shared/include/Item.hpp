#pragma once

#include <string>
#include <cstdint>

namespace inventory {

struct ItemSize {
    int width;
    int height;
    
    ItemSize() : width(1), height(1) {}
    ItemSize(int w, int h) : width(w), height(h) {}
};

class Item {
public:
    Item();
    Item(uint32_t id, const std::string& name, ItemSize size, uint32_t stackLimit, const std::string& imagePath);
    
    uint32_t getId() const { return id_; }
    const std::string& getName() const { return name_; }
    ItemSize getSize() const { return size_; }
    uint32_t getStackLimit() const { return stackLimit_; }
    const std::string& getImagePath() const { return imagePath_; }
    
    // serialization helpers
    std::string serialize() const;
    static Item deserialize(const std::string& data);
    
private:
    uint32_t id_;
    std::string name_;
    ItemSize size_;
    uint32_t stackLimit_;
    std::string imagePath_;
};

} // namespace inventory
