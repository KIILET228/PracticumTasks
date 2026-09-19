#pragma once

#include <boost/json.hpp>
#include <unordered_map>

#include "model.h"
#include "tagged.h"

namespace extra_data {

// Хранит данные, нужные только фронтенду (например, описание типов трофеев),
// и не влияющие на игровую логику. Это позволяет не тащить в model.h
// зависимость от конкретной JSON-библиотеки/формата хранения.
class LootTypesInfo {
public:
    void Add(const model::Map::Id& map_id, boost::json::array loot_types) {
        data_[map_id] = std::move(loot_types);
    }

    const boost::json::array* Find(const model::Map::Id& map_id) const {
        if (auto it = data_.find(map_id); it != data_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<model::Map::Id, boost::json::array, util::TaggedHasher<model::Map::Id>> data_;
};

}  // namespace extra_data
