#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

constexpr double kRoadHalfWidth = 0.4;
constexpr double kEps = 1e-9;

std::mt19937& GetRandomEngine() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}

double NextUniform01() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(GetRandomEngine());
}

// Реальный (не тестовый) генератор псевдослучайных чисел в диапазоне [0, 1),
// используемый и для генератора трофеев, и для выбора точек на дорогах.
std::function<double()> MakeRealRandomSource() {
    return [] {
        return NextUniform01();
    };
}

// Выбирает точку на дороге по параметру t из [0, 1): 0 соответствует началу
// дороги, значения ближе к 1 - концу дороги.
Position PointOnRoad(const Road& road, double t) {
    const Point start = road.GetStart();
    const Point end = road.GetEnd();

    if (road.IsHorizontal()) {
        return Position{static_cast<double>(start.x) + t * (end.x - start.x), static_cast<double>(start.y)};
    }
    return Position{static_cast<double>(start.x), static_cast<double>(start.y) + t * (end.y - start.y)};
}

}  // namespace

Bounds Map::ComputeBounds(Position pos) const noexcept {
    bool found = false;
    Bounds bounds{};

    for (const auto& road : roads_) {
        const Point start = road.GetStart();
        const Point end = road.GetEnd();

        double rx0, rx1, ry0, ry1;
        if (road.IsHorizontal()) {
            const auto [lo, hi] = std::minmax(start.x, end.x);
            rx0 = static_cast<double>(lo) - kRoadHalfWidth;
            rx1 = static_cast<double>(hi) + kRoadHalfWidth;
            ry0 = static_cast<double>(start.y) - kRoadHalfWidth;
            ry1 = static_cast<double>(start.y) + kRoadHalfWidth;
        } else {
            const auto [lo, hi] = std::minmax(start.y, end.y);
            rx0 = static_cast<double>(start.x) - kRoadHalfWidth;
            rx1 = static_cast<double>(start.x) + kRoadHalfWidth;
            ry0 = static_cast<double>(lo) - kRoadHalfWidth;
            ry1 = static_cast<double>(hi) + kRoadHalfWidth;
        }

        if (pos.x < rx0 - kEps || pos.x > rx1 + kEps || pos.y < ry0 - kEps || pos.y > ry1 + kEps) {
            continue;
        }

        if (!found) {
            bounds = Bounds{rx0, rx1, ry0, ry1};
            found = true;
        } else {
            bounds.min_x = std::min(bounds.min_x, rx0);
            bounds.max_x = std::max(bounds.max_x, rx1);
            bounds.min_y = std::min(bounds.min_y, ry0);
            bounds.max_y = std::max(bounds.max_y, ry1);
        }
    }

    if (!found) {
        return Bounds{pos.x, pos.x, pos.y, pos.y};
    }
    return bounds;
}

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

    if (!randomize_spawn_points_) {
        const Point start = roads.front().GetStart();
        return Position{static_cast<double>(start.x), static_cast<double>(start.y)};
    }

    size_t road_index = static_cast<size_t>(uniform_gen_() * roads.size());
    if (road_index >= roads.size()) {
        road_index = roads.size() - 1;
    }
    const double t = uniform_gen_();
    return PointOnRoad(roads[road_index], t);
}

void GameSession::GenerateLoot(TimeInterval time_delta) {
    const auto loot_count = static_cast<unsigned>(lost_objects_.size());
    const auto looter_count = static_cast<unsigned>(dogs_.size());
    const unsigned generated = loot_generator_.Generate(time_delta, loot_count, looter_count);

    if (generated == 0) {
        return;
    }

    const auto& roads = map_.GetRoads();
    const size_t loot_types_count = map_.GetLootTypesCount();
    if (roads.empty() || loot_types_count == 0) {
        return;
    }

    for (unsigned i = 0; i < generated; ++i) {
        size_t road_index = static_cast<size_t>(uniform_gen_() * roads.size());
        if (road_index >= roads.size()) {
            road_index = roads.size() - 1;
        }
        const double t = uniform_gen_();
        const Position position = PointOnRoad(roads[road_index], t);

        unsigned type = static_cast<unsigned>(uniform_gen_() * loot_types_count);
        if (type >= loot_types_count) {
            type = static_cast<unsigned>(loot_types_count) - 1;
        }

        LostObject::Id id{next_loot_id_++};
        lost_objects_.emplace_back(id, type, position);
    }
}

void GameSession::Move(double dt_seconds) {
    for (Dog& dog : dogs_) {
        const Speed speed = dog.GetSpeed();
        if (speed.vx == 0.0 && speed.vy == 0.0) {
            continue;
        }

        const Position pos = dog.GetPosition();
        const Position unclamped{pos.x + speed.vx * dt_seconds, pos.y + speed.vy * dt_seconds};

        const Bounds bounds = map_.ComputeBounds(pos);

        Position result = unclamped;
        bool hit_boundary = false;

        if (result.x < bounds.min_x) {
            result.x = bounds.min_x;
            hit_boundary = true;
        } else if (result.x > bounds.max_x) {
            result.x = bounds.max_x;
            hit_boundary = true;
        }

        if (result.y < bounds.min_y) {
            result.y = bounds.min_y;
            hit_boundary = true;
        } else if (result.y > bounds.max_y) {
            result.y = bounds.max_y;
            hit_boundary = true;
        }

        dog.SetPosition(result);
        if (hit_boundary) {
            dog.SetSpeed(Speed{0.0, 0.0});
        }
    }
}

Dog& GameSession::AddDog(std::string name) {
    Dog::Id id{next_dog_id_++};
    return dogs_.emplace_back(id, std::move(name), GenerateStartPosition());
}

void Game::SetLootGeneratorConfig(std::chrono::milliseconds period, double probability) {
    loot_period_ = period;
    loot_probability_ = probability;
}

void Game::Tick(std::chrono::milliseconds delta) {
    const double dt_seconds = std::chrono::duration<double>(delta).count();
    for (auto& session : sessions_) {
        session.Move(dt_seconds);
        session.GenerateLoot(delta);
    }
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
    sessions_.emplace_back(*map, loot_period_, loot_probability_, MakeRealRandomSource(), MakeRealRandomSource(),
                           randomize_spawn_points_);
    map_id_to_session_index_.emplace(map_id, index);
    return sessions_.back();
}

}  // namespace model
