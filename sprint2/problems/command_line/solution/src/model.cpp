#include "model.h"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

constexpr double kRoadHalfWidth = 0.4;

constexpr double kEps = 1e-9;

}

namespace {

std::mt19937& GetRandomEngine() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}

double RandomInRange(Coord lo, Coord hi) {
    if (lo == hi) {
        return static_cast<double>(lo);
    }
    std::uniform_real_distribution<double> dist(static_cast<double>(lo), static_cast<double>(hi));
    return dist(GetRandomEngine());
}

}

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

void Game::Tick(std::chrono::milliseconds delta) {
    const double dt_seconds = std::chrono::duration<double>(delta).count();
    for (auto& session : sessions_) {
        session.Move(dt_seconds);
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
    sessions_.emplace_back(*map, randomize_spawn_points_);
    map_id_to_session_index_.emplace(map_id, index);
    return sessions_.back();
}

}
