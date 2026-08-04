// 1
#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

namespace {

std::string ReadFileContents(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open config file: "s + path.string());
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
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

model::Map ParseMap(const json::object& obj) {
    model::Map::Id id{std::string(obj.at("id").as_string())};
    std::string name{obj.at("name").as_string()};
    model::Map map(std::move(id), std::move(name));

    for (const auto& road_value : obj.at("roads").as_array()) {
        map.AddRoad(ParseRoad(road_value.as_object()));
    }
    for (const auto& building_value : obj.at("buildings").as_array()) {
        map.AddBuilding(ParseBuilding(building_value.as_object()));
    }
    for (const auto& office_value : obj.at("offices").as_array()) {
        map.AddOffice(ParseOffice(office_value.as_object()));
    }

    return map;
}

}

model::Game LoadGame(const std::filesystem::path& json_path) {
    const auto content = ReadFileContents(json_path);
    const auto value = json::parse(content);
    const auto& root = value.as_object();

    model::Game game;
    for (const auto& map_value : root.at("maps").as_array()) {
        game.AddMap(ParseMap(map_value.as_object()));
    }
    return game;
}

}
