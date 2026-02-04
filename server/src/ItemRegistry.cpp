#include "ItemRegistry.hpp"
#include <iostream>

namespace inventory
{

    ItemRegistry &ItemRegistry::getInstance()
    {
        static ItemRegistry instance;
        return instance;
    }

    void ItemRegistry::initialize()
    {
        // clear existing items
        items_.clear();


        // RegisterItem(id, name, size(width, height), stackLimit, imagePath)
        // The images are being set on the client with the item id
        registerItem(std::make_shared<Item>(1, "Chaos Orb", ItemSize{1, 1}, 20, ""));
        registerItem(std::make_shared<Item>(2, "Divine Orb", ItemSize{1, 1}, 20, ""));
        registerItem(std::make_shared<Item>(3, "Exalted Orb", ItemSize{1, 1}, 20, ""));
        registerItem(std::make_shared<Item>(4, "Orb of Alteration", ItemSize{1, 1}, 20, ""));
        registerItem(std::make_shared<Item>(5, "Scroll of Wisdom", ItemSize{1, 1}, 40, ""));

        // a few uniques
        registerItem(std::make_shared<Item>(6, "Starforge", ItemSize{2, 4}, 1, ""));
        registerItem(std::make_shared<Item>(7, "Voltaxic Rift", ItemSize{2, 4}, 1, ""));
        registerItem(std::make_shared<Item>(8, "Starkonja", ItemSize{2, 2}, 1, ""));
        registerItem(std::make_shared<Item>(9, "Facebreaker", ItemSize{2, 2}, 1, ""));
        registerItem(std::make_shared<Item>(10, "Volls Protector", ItemSize{2, 3}, 1, ""));
        registerItem(std::make_shared<Item>(11, "Blood Dance", ItemSize{2, 2}, 1, ""));
        registerItem(std::make_shared<Item>(12, "Call of the Brotherhood", ItemSize{1, 1}, 1, ""));

        std::cout << "ItemRegistry initialized with " << items_.size() << " items" << std::endl;
    }

    void ItemRegistry::registerItem(std::shared_ptr<Item> item)
    {
        items_[item->getId()] = item;
    }

    std::shared_ptr<Item> ItemRegistry::getItem(uint32_t id) const
    {
        auto it = items_.find(id);
        if (it != items_.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Item>> ItemRegistry::getAllItems() const
    {
        std::vector<std::shared_ptr<Item>> result;
        result.reserve(items_.size());
        for (const auto &[id, item] : items_)
        {
            result.push_back(item);
        }
        return result;
    }

} // namespace inventory
