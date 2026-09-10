#pragma once

#include <boost/json.hpp>
#include <unordered_map>

#include "model.h"
#include "tagged.h"

// Frontend-facing information that has no bearing on game logic and
// therefore doesn't belong in the model (see the assignment notes on why
// lootTypes shouldn't live inside model::Map). It's read verbatim from the
// game config JSON at startup and served back to clients as-is by the
// /api/v1/maps/{id} endpoint.
namespace extra_data {

class LootTypesInfo {
public:
    void AddMapLootTypes(const model::Map::Id& map_id, boost::json::array loot_types) {
        loot_types_by_map_.insert_or_assign(map_id, std::move(loot_types));
    }

    // Returns a pointer to the loot type descriptions for the given map,
    // or nullptr if the map has none registered.
    const boost::json::array* FindLootTypes(const model::Map::Id& map_id) const {
        if (auto it = loot_types_by_map_.find(map_id); it != loot_types_by_map_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<model::Map::Id, boost::json::array, util::TaggedHasher<model::Map::Id>> loot_types_by_map_;
};

}  // namespace extra_data
