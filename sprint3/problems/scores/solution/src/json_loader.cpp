#include "json_loader.h"

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

namespace {

constexpr double kDefaultDogSpeed = 1.0;
constexpr unsigned kDefaultBagCapacity = 3;

std::string ReadFileContents(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open config file: "s + path.string());
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

double ToDouble(const json::value& value) {
    if (value.is_double()) {
        return value.as_double();
    }
    if (value.is_int64()) {
        return static_cast<double>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<double>(value.as_uint64());
    }
    throw std::runtime_error("Expected a number");
}

unsigned ToUnsigned(const json::value& value) {
    if (value.is_int64()) {
        return static_cast<unsigned>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<unsigned>(value.as_uint64());
    }
    throw std::runtime_error("Expected an unsigned integer");
}

model::Road ParseRoad(const json::object& obj) {
    model::Point start{static_cast<model::Coord>(obj.at("x0").as_int64()),
                        static_cast<model::Coord>(obj.at("y0").as_int64())};
    if (const auto* x1 = obj.if_contains("x1")) {
        return model::Road(model::Road::HORIZONTAL, start, static_cast<model::Coord>(x1->as_int64()));
    }
    return model::Road(model::Road::VERTICAL, start,
                       static_cast<model::Coord>(obj.at("y1").as_int64()));
}

model::Building ParseBuilding(const json::object& obj) {
    model::Rectangle bounds{
        {static_cast<model::Coord>(obj.at("x").as_int64()), static_cast<model::Coord>(obj.at("y").as_int64())},
        {static_cast<model::Dimension>(obj.at("w").as_int64()),
         static_cast<model::Dimension>(obj.at("h").as_int64())}};
    return model::Building(bounds);
}

model::Office ParseOffice(const json::object& obj) {
    model::Office::Id id{std::string(obj.at("id").as_string())};
    model::Point position{static_cast<model::Coord>(obj.at("x").as_int64()),
                          static_cast<model::Coord>(obj.at("y").as_int64())};
    model::Offset offset{static_cast<model::Dimension>(obj.at("offsetX").as_int64()),
                         static_cast<model::Dimension>(obj.at("offsetY").as_int64())};
    return model::Office(std::move(id), position, offset);
}

model::Map ParseMap(const json::object& obj, double default_dog_speed, unsigned default_bag_capacity,
                    extra_data::LootTypesInfo& loot_types_info) {
    std::string id_str{obj.at("id").as_string()};
    model::Map::Id id{id_str};
    std::string name{obj.at("name").as_string()};
    model::Map map(std::move(id), std::move(name));

    double dog_speed = default_dog_speed;
    if (const auto* dog_speed_value = obj.if_contains("dogSpeed")) {
        dog_speed = ToDouble(*dog_speed_value);
    }
    map.SetDogSpeed(dog_speed);

    unsigned bag_capacity = default_bag_capacity;
    if (const auto* bag_capacity_value = obj.if_contains("bagCapacity")) {
        bag_capacity = ToUnsigned(*bag_capacity_value);
    }
    map.SetBagCapacity(bag_capacity);

    try {
        for (const auto& road_value : obj.at("roads").as_array()) {
            map.AddRoad(ParseRoad(road_value.as_object()));
        }
        for (const auto& building_value : obj.at("buildings").as_array()) {
            map.AddBuilding(ParseBuilding(building_value.as_object()));
        }
        for (const auto& office_value : obj.at("offices").as_array()) {
            map.AddOffice(ParseOffice(office_value.as_object()));
        }

        const auto& loot_types_array = obj.at("lootTypes").as_array();
        if (loot_types_array.empty()) {
            throw std::runtime_error("lootTypes must contain at least one loot type");
        }
        map.SetLootTypesCount(loot_types_array.size());

        std::vector<unsigned> loot_values;
        loot_values.reserve(loot_types_array.size());
        for (const auto& loot_type_value : loot_types_array) {
            const auto& loot_type_obj = loot_type_value.as_object();
            unsigned value = 0;
            if (const auto* value_ptr = loot_type_obj.if_contains("value")) {
                value = ToUnsigned(*value_ptr);
            }
            loot_values.push_back(value);
        }
        map.SetLootValues(std::move(loot_values));

        loot_types_info.Add(map.GetId(), loot_types_array);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to parse map \""s + id_str + "\": "s + ex.what());
    }

    return map;
}

}

GameData LoadGame(const std::filesystem::path& json_path) {
    GameData game_data;
    try {
        const auto content = ReadFileContents(json_path);
        const auto value = json::parse(content);
        const auto& root = value.as_object();

        double default_dog_speed = kDefaultDogSpeed;
        if (const auto* default_dog_speed_value = root.if_contains("defaultDogSpeed")) {
            default_dog_speed = ToDouble(*default_dog_speed_value);
        }

        unsigned default_bag_capacity = kDefaultBagCapacity;
        if (const auto* default_bag_capacity_value = root.if_contains("defaultBagCapacity")) {
            default_bag_capacity = ToUnsigned(*default_bag_capacity_value);
        }

        for (const auto& map_value : root.at("maps").as_array()) {
            game_data.game.AddMap(
                ParseMap(map_value.as_object(), default_dog_speed, default_bag_capacity, game_data.loot_types_info));
        }

        const auto& loot_gen_cfg = root.at("lootGeneratorConfig").as_object();
        const double period_s = ToDouble(loot_gen_cfg.at("period"));
        const double probability = ToDouble(loot_gen_cfg.at("probability"));
        const auto period = std::chrono::milliseconds(static_cast<std::int64_t>(period_s * 1000));
        game_data.game.SetLootGeneratorConfig(period, probability);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to load game config from "s + json_path.string() + ": "s + ex.what());
    }
    return game_data;
}

}
