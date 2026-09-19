#pragma once

#include <filesystem>

#include "extra_data.h"
#include "model.h"

// 1

namespace json_loader {

struct GameData {
    model::Game game;
    extra_data::LootTypesInfo loot_types_info;
};

GameData LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
