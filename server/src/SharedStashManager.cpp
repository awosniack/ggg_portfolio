#include "SharedStashManager.hpp"

namespace inventory {

SharedStashManager::SharedStashManager() {
    // init 3 shared stashes (12x12 each)
    for (int i = 0; i < 3; ++i) {
        stashes_[i] = std::make_shared<Inventory>(12, 12);
    }
}

SharedStashManager::~SharedStashManager() {
}

std::shared_ptr<Inventory> SharedStashManager::getSharedStash(int stashIndex) {
    if (stashIndex >= 0 && stashIndex < 3) {
        return stashes_[stashIndex];
    }
    return nullptr;
}

} // namespace inventory
