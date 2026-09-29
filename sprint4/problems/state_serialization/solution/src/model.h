#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "geom.h"
#include "tagged.h"

namespace model {

// ============================================================
//  Базовые геометрические типы
// ============================================================

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
    constexpr auto operator<=>(const Point&) const = default;
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

// Алиасы, используемые в app.h/api_handler.cpp
using Position = geom::Point2D;

struct Speed {
    double vx = 0.0;
    double vy = 0.0;

    Speed() = default;
    Speed(double vx_, double vy_) : vx(vx_), vy(vy_) {}

    // Совместимость с geom::Vec2D (Dog хранит именно geom::Vec2D).
    Speed(const geom::Vec2D& v) : vx(v.x), vy(v.y) {}
    operator geom::Vec2D() const { return {vx, vy}; }

    auto operator<=>(const Speed&) const = default;
};

// ============================================================
//  Потерянные / подобранные объекты
// ============================================================

using LostObjectType = unsigned;
using Score = unsigned;

struct FoundObject {
    using Id = util::Tagged<uint32_t, FoundObject>;

    Id id{0u};
    LostObjectType type{0u};

    [[nodiscard]] auto operator<=>(const FoundObject&) const = default;
};

class LostObject {
public:
    using Id = util::Tagged<uint32_t, LostObject>;

    LostObject() = default;
    LostObject(Id id, LostObjectType type, geom::Point2D pos)
        : id_(id)
        , type_(type)
        , position_(pos) {
    }

    const Id& GetId() const noexcept { return id_; }
    LostObjectType GetType() const noexcept { return type_; }
    const geom::Point2D& GetPosition() const noexcept { return position_; }

private:
    Id id_{0u};
    LostObjectType type_ = 0;
    geom::Point2D position_;
};

// ============================================================
//  Направление и собака
// ============================================================

enum class Direction {
    NORTH,
    EAST,
    WEST,
    SOUTH,
};

class Dog {
public:
    using Id = util::Tagged<uint32_t, Dog>;
    using BagContent = std::vector<FoundObject>;

    Dog(Id id, std::string name, geom::Point2D pos, size_t bag_cap)
        : id_(std::move(id))
        , name_(std::move(name))
        , position_(pos)
        , bag_cap_(bag_cap) {
        bag_.reserve(bag_cap);
    }

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const geom::Point2D& GetPosition() const noexcept { return position_; }
    const geom::Vec2D& GetSpeed() const noexcept { return speed_; }

    void SetSpeed(geom::Vec2D speed) noexcept { speed_ = speed; }
    void SetPosition(geom::Point2D position) noexcept { position_ = position; }
    void SetDirection(Direction d) noexcept { direction_ = d; }
    Direction GetDirection() const noexcept { return direction_; }

    size_t GetBagCapacity() const noexcept { return bag_cap_; }
    Score GetScore() const noexcept { return score_; }
    void AddScore(Score s) noexcept { score_ += s; }

    [[nodiscard]] bool PutToBag(FoundObject item) {
        if (IsBagFull()) return false;
        bag_.push_back(item);
        return true;
    }

    size_t EmptyBag() noexcept {
        auto n = bag_.size();
        bag_.clear();
        return n;
    }

    bool IsBagFull() const noexcept { return bag_.size() >= bag_cap_; }
    const BagContent& GetBag() const noexcept { return bag_; }
    const BagContent& GetBagContent() const noexcept { return bag_; }

    // Пенсия (для save/load состояния).
    void SetRetired() noexcept { retired_ = true; }
    bool IsRetired() const noexcept { return retired_; }

private:
    Id id_;
    std::string name_;
    geom::Point2D position_;
    geom::Vec2D speed_;
    Direction direction_{Direction::NORTH};
    BagContent bag_;
    size_t bag_cap_;
    Score score_{};
    bool retired_ = false;
};

using DogPtr = std::shared_ptr<Dog>;
using ConstDogPtr = std::shared_ptr<const Dog>;

// ============================================================
//  Компоненты карты
// ============================================================

class Road {
public:
    enum Orientation { HORIZONTAL, VERTICAL };

    Road() = default;
    Road(Orientation orientation, Point start, Coord end)
        : orientation_(orientation)
        , start_(start)
        , end_coord_(end) {
    }

    bool IsHorizontal() const noexcept { return orientation_ == HORIZONTAL; }
    const Point& GetStart() const noexcept { return start_; }

    Point GetEnd() const noexcept {
        if (IsHorizontal()) return {end_coord_, start_.y};
        return {start_.x, end_coord_};
    }

    Orientation GetOrientation() const noexcept { return orientation_; }

private:
    Orientation orientation_ = HORIZONTAL;
    Point start_{};
    Coord end_coord_ = 0;
};

class Building {
public:
    explicit Building(Rectangle bounds) : bounds_(bounds) {}

    const Rectangle& GetBounds() const noexcept { return bounds_; }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset)
        : id_(std::move(id))
        , position_(position)
        , offset_(offset) {
    }

    const Id& GetId() const noexcept { return id_; }
    Point GetPosition() const noexcept { return position_; }
    Offset GetOffset() const noexcept { return offset_; }

private:
    Id id_;
    Point position_{};
    Offset offset_{};
};

// ============================================================
//  Карта
// ============================================================

class Map {
public:
    using Id = util::Tagged<std::string, Map>;

    Map(Id id, std::string name)
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }

    const std::vector<Road>& GetRoads() const noexcept { return roads_; }
    const std::vector<Building>& GetBuildings() const noexcept { return buildings_; }
    const std::vector<Office>& GetOffices() const noexcept { return offices_; }

    void AddRoad(Road r) { roads_.push_back(std::move(r)); }
    void AddBuilding(Building b) { buildings_.push_back(std::move(b)); }
    void AddOffice(Office o) { offices_.push_back(std::move(o)); }

    void SetDogSpeed(double s) noexcept { dog_speed_ = s; }
    double GetDogSpeed() const noexcept { return dog_speed_; }

    void SetBagCapacity(unsigned c) noexcept { bag_capacity_ = c; }
    unsigned GetBagCapacity() const noexcept { return bag_capacity_; }

    void SetLootTypesCount(size_t c) noexcept { loot_types_count_ = c; }
    size_t GetLootTypesCount() const noexcept { return loot_types_count_; }

    void SetLootValues(std::vector<unsigned> v) { loot_values_ = std::move(v); }
    const std::vector<unsigned>& GetLootValues() const noexcept { return loot_values_; }

    // Точка появления собаки (позиция первого офиса).
    std::optional<geom::Point2D> GetSpawnPoint() const {
        if (offices_.empty()) return std::nullopt;
        const auto& p = offices_.front().GetPosition();
        return geom::Point2D{static_cast<double>(p.x), static_cast<double>(p.y)};
    }

private:
    Id id_;
    std::string name_;
    std::vector<Road> roads_;
    std::vector<Building> buildings_;
    std::vector<Office> offices_;

    double dog_speed_ = 1.0;
    unsigned bag_capacity_ = 3;
    size_t loot_types_count_ = 0;
    std::vector<unsigned> loot_values_;
};

// ============================================================
//  Игровая сессия
// ============================================================

class GameSession {
public:
    explicit GameSession(const Map& map) : map_(map) {}

    const Map& GetMap() const noexcept { return map_; }

    Dog& AddDog(const std::string& name) {
        const auto pos = map_.GetSpawnPoint().value_or(geom::Point2D{0.0, 0.0});
        dogs_.push_back(std::make_shared<Dog>(
            Dog::Id{next_dog_id_++}, name, pos, map_.GetBagCapacity()));
        return *dogs_.back();
    }

    const std::vector<DogPtr>& GetDogs() const noexcept { return dogs_; }

    const std::unordered_map<LostObject::Id, LostObject,
                             util::TaggedHasher<LostObject::Id>>&
    GetLostObjects() const noexcept {
        return lost_objects_;
    }

    // Восстановление из сохранённого состояния.
    void AddRestoredDog(Dog dog) {
        const auto id_val = *dog.GetId();
        if (id_val >= next_dog_id_) next_dog_id_ = id_val + 1;
        dogs_.push_back(std::make_shared<Dog>(std::move(dog)));
    }

    void AddRestoredLostObject(LostObject obj) {
        const auto id_val = *obj.GetId();
        if (id_val >= next_loot_id_) next_loot_id_ = id_val + 1;
        lost_objects_.emplace(obj.GetId(), std::move(obj));
    }

    Dog* FindDog(const Dog::Id& id) noexcept {
        for (auto& d : dogs_) {
            if (d->GetId() == id) return d.get();
        }
        return nullptr;
    }

    // Тик сессии (движение, сбор трофеев, генерация). Реализация в model.cpp.
    void Tick(std::chrono::milliseconds delta);

private:
    const Map& map_;
    std::vector<DogPtr> dogs_;
    std::unordered_map<LostObject::Id, LostObject,
                       util::TaggedHasher<LostObject::Id>>
        lost_objects_;
    uint32_t next_dog_id_ = 0;
    uint32_t next_loot_id_ = 0;
};

// ============================================================
//  Игра — контейнер карт и сессий
// ============================================================

class Game {
public:
    Game() = default;

    void AddMap(Map map) {
        const auto id = map.GetId();
        map_id_to_index_[id] = maps_.size();
        maps_.push_back(std::move(map));
    }

    const std::vector<Map>& GetMaps() const noexcept { return maps_; }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_[it->second];
        }
        return nullptr;
    }

    GameSession& JoinSession(const Map::Id& id) {
        if (auto it = sessions_.find(id); it != sessions_.end()) {
            return it->second;
        }
        const auto* map = FindMap(id);
        if (!map) {
            throw std::runtime_error("Map not found");
        }
        auto [it, _] = sessions_.emplace(id, GameSession{*map});
        return it->second;
    }

    const std::unordered_map<Map::Id, GameSession,
                             util::TaggedHasher<Map::Id>>&
    GetSessions() const noexcept {
        return sessions_;
    }

    // Полный сброс сессий (используется при загрузке состояния).
    void ResetSessions() noexcept { sessions_.clear(); }

    void Tick(std::chrono::milliseconds delta) {
        for (auto& [_, session] : sessions_) {
            session.Tick(delta);
        }
    }

    void SetDogRetirementTime(std::chrono::milliseconds t) noexcept {
        dog_retirement_time_ = t;
    }
    std::chrono::milliseconds GetDogRetirementTime() const noexcept {
        return dog_retirement_time_;
    }

    void SetLootGeneratorConfig(std::chrono::milliseconds period, double prob) noexcept {
        loot_period_ = period;
        loot_probability_ = prob;
    }
    std::chrono::milliseconds GetLootPeriod() const noexcept { return loot_period_; }
    double GetLootProbability() const noexcept { return loot_probability_; }

    void SetRandomizeSpawnPoints(bool v) noexcept { randomize_spawn_points_ = v; }
    bool GetRandomizeSpawnPoints() const noexcept { return randomize_spawn_points_; }

private:
    std::vector<Map> maps_;
    std::unordered_map<Map::Id, size_t, util::TaggedHasher<Map::Id>> map_id_to_index_;
    std::unordered_map<Map::Id, GameSession, util::TaggedHasher<Map::Id>> sessions_;

    std::chrono::milliseconds dog_retirement_time_{60000};
    std::chrono::milliseconds loot_period_{5000};
    double loot_probability_ = 0.5;
    bool randomize_spawn_points_ = false;
};

}  // namespace model