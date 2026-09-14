#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Position {
    double x = 0;
    double y = 0;
};

struct Speed {
    double vx = 0;
    double vy = 0;
};

struct Bounds {
    double min_x = 0;
    double max_x = 0;
    double min_y = 0;
    double max_y = 0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST,
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    void SetDogSpeed(double dog_speed) noexcept {
        dog_speed_ = dog_speed;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    Bounds ComputeBounds(Position pos) const noexcept;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_ = 1.0;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    using Id = util::Tagged<std::uint64_t, Dog>;

    Dog(Id id, std::string name, Position position) noexcept
        : id_(id)
        , name_(std::move(name))
        , position_(position) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    Position GetPosition() const noexcept {
        return position_;
    }

    void SetPosition(Position position) noexcept {
        position_ = position;
    }

    Speed GetSpeed() const noexcept {
        return speed_;
    }

    void SetSpeed(Speed speed) noexcept {
        speed_ = speed;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetDirection(Direction direction) noexcept {
        direction_ = direction;
    }

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_{};
    Direction direction_ = Direction::NORTH;
};

class GameSession {
public:
    GameSession(const Map& map, bool randomize_spawn_points) noexcept
        : map_(map)
        , randomize_spawn_points_(randomize_spawn_points) {
    }

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    const Map& GetMap() const noexcept {
        return map_;
    }

    Dog& AddDog(std::string name);

    const std::deque<Dog>& GetDogs() const noexcept {
        return dogs_;
    }

    void Move(double dt_seconds);

private:
    Position GenerateStartPosition() const;

    const Map& map_;
    bool randomize_spawn_points_;
    std::deque<Dog> dogs_;
    std::uint64_t next_dog_id_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession& JoinSession(const Map::Id& map_id);

    void Tick(std::chrono::milliseconds delta);

    void SetRandomizeSpawnPoints(bool randomize_spawn_points) noexcept {
        randomize_spawn_points_ = randomize_spawn_points;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    std::deque<GameSession> sessions_;
    MapIdToIndex map_id_to_session_index_;
    bool randomize_spawn_points_ = false;
};

}
