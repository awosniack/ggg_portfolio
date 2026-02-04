#include "Item.hpp"
#include <sstream>

namespace inventory {

Item::Item() 
    : id_(0), name_(""), size_(1, 1), stackLimit_(1), imagePath_("") {
}

Item::Item(uint32_t id, const std::string& name, ItemSize size, uint32_t stackLimit, const std::string& imagePath)
    : id_(id), name_(name), size_(size), stackLimit_(stackLimit), imagePath_(imagePath) {
}

std::string Item::serialize() const {
    std::ostringstream oss;
    oss << id_ << "|" << name_ << "|" << size_.width << "," << size_.height 
        << "|" << stackLimit_;
    return oss.str();
}

Item Item::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    
    uint32_t id;
    std::string name;
    ItemSize size;
    uint32_t stackLimit;
    
    std::getline(iss, token, '|');
    id = std::stoul(token);
    
    std::getline(iss, name, '|');
    
    std::getline(iss, token, '|');
    size_t comma = token.find(',');
    size.width = std::stoi(token.substr(0, comma));
    size.height = std::stoi(token.substr(comma + 1));
    
    std::getline(iss, token, '|');
    stackLimit = std::stoul(token);
    
    // image path will be resolved client-side based on item ID
    return Item(id, name, size, stackLimit, "");
}

} // namespace inventory
