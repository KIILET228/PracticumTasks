#pragma once

#include <filesystem>

#include "extra_data.h"
#include "model.h"

namespace json_loader {

// Everything obtained from parsing the game config JSON: the game model
// itself, plus the frontend-only loot type descriptions that the model
// doesn't need to know about.
struct GameData {
    model::Game game;
    extra_data::LootTypesInfo extra_data;
};

GameData LoadGame(const std::filesystem::path& json_path);

}
