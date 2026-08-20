#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

std::mt19937& GetRandomEngine() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}

// std::uniform_real_distribution requires lo < hi; roads may be zero-length.
double RandomInRange(Coord lo, Coord hi) {
    if (lo == hi) {
        return static_cast<double>(lo);
    }
    std::uniform_real_distribution<double> dist(static_cast<double>(lo), static_cast<double>(hi));
    return dist(GetRandomEngine());
}

}  // namespace

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

Position GameSession::GenerateStartPosition() const {
    const auto& roads = map_.GetRoads();
    if (roads.empty()) {
        return Position{0.0, 0.0};
    }

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const Road& road = roads[road_dist(GetRandomEngine())];

    const Point start = road.GetStart();
    const Point end = road.GetEnd();

    if (road.IsHorizontal()) {
        const auto [lo, hi] = std::minmax(start.x, end.x);
        return Position{RandomInRange(lo, hi), static_cast<double>(start.y)};
    }

    const auto [lo, hi] = std::minmax(start.y, end.y);
    return Position{static_cast<double>(start.x), RandomInRange(lo, hi)};
}

Dog& GameSession::AddDog(std::string name) {
    Dog::Id id{next_dog_id_++};
    return dogs_.emplace_back(id, std::move(name), GenerateStartPosition());
}

GameSession& Game::JoinSession(const Map::Id& map_id) {
    if (auto it = map_id_to_session_index_.find(map_id); it != map_id_to_session_index_.end()) {
        return sessions_[it->second];
    }

    const Map* map = FindMap(map_id);
    if (!map) {
        throw std::invalid_argument("Map not found: "s + *map_id);
    }

    const size_t index = sessions_.size();
    sessions_.emplace_back(*map);
    map_id_to_session_index_.emplace(map_id, index);
    return sessions_.back();
}

}
