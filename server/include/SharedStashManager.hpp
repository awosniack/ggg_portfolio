#pragma once

#include "Inventory.hpp"
#include <mutex>
#include <memory>

namespace inventory {

class SharedStashManager {
public:
    SharedStashManager();
    ~SharedStashManager();
    
    std::shared_ptr<Inventory> getSharedStash(int stashIndex);
    
private:
    std::shared_ptr<Inventory> stashes_[3];
    std::mutex stashMutexes_[3];
};

} // namespace inventory
