#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
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

    // Количество различных типов трофеев (lootTypes), заданных для этой карты
    // в конфигурационном файле. Само содержимое lootTypes (нужное лишь фронтенду)
    // модель не хранит - см. модуль extra_data.
    unsigned GetLootTypesCount() const noexcept {
        return loot_types_count_;
    }

    void SetLootTypesCount(unsigned loot_types_count) noexcept {
        loot_types_count_ = loot_types_count;
    }

    // Вместимость рюкзака собак на этой карте.
    unsigned GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(unsigned bag_capacity) noexcept {
        bag_capacity_ = bag_capacity;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_ = 1.0;
    unsigned loot_types_count_ = 0;
    unsigned bag_capacity_ = 3;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class LostObject {
public:
    using Id = util::Tagged<std::uint64_t, LostObject>;

    LostObject(Id id, unsigned type, Position position) noexcept
        : id_(id)
        , type_(type)
        , position_(position) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    unsigned GetType() const noexcept {
        return type_;
    }

    Position GetPosition() const noexcept {
        return position_;
    }

private:
    Id id_;
    unsigned type_;
    Position position_;
};

// Предмет, лежащий в рюкзаке собаки. id и type совпадают с теми, что были у
// предмета до того, как его подобрали (см. LostObject).
struct BagItem {
    LostObject::Id id;
    unsigned type;
};

class Dog {
public:
    using Id = util::Tagged<std::uint64_t, Dog>;

    Dog(Id id, std::string name, Position position, unsigned bag_capacity) noexcept
        : id_(id)
        , name_(std::move(name))
        , position_(position)
        , bag_capacity_(bag_capacity) {
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

    const std::vector<BagItem>& GetBag() const noexcept {
        return bag_;
    }

    bool IsBagFull() const noexcept {
        return bag_.size() >= bag_capacity_;
    }

    // Пытается положить предмет в рюкзак. Возвращает false, если рюкзак
    // уже полон - в этом случае предмет остаётся не собранным.
    bool TryPutInBag(LostObject::Id id, unsigned type) {
        if (IsBagFull()) {
            return false;
        }
        bag_.push_back(BagItem{id, type});
        return true;
    }

    // Опустошает рюкзак - все предметы считаются сданными на базу.
    void ClearBag() noexcept {
        bag_.clear();
    }

private:
    Id id_;
    std::string name_;
    Position position_;
    Speed speed_{};
    Direction direction_ = Direction::NORTH;
    std::vector<BagItem> bag_;
    unsigned bag_capacity_;
};

class GameSession {
public:
    using TimeInterval = loot_gen::LootGenerator::TimeInterval;
    // Генератор псевдослучайных чисел в диапазоне [0, 1), используемый для
    // выбора дороги, точки на дороге и типа трофея при генерации потерянных
    // предметов. Вынесен отдельно от LootGenerator::RandomGenerator, чтобы
    // тесты могли независимо контролировать оба источника случайности.
    using UniformRandomGenerator = std::function<double()>;

    GameSession(const Map& map, TimeInterval loot_base_interval, double loot_probability,
               loot_gen::LootGenerator::RandomGenerator loot_random_gen, UniformRandomGenerator uniform_gen,
               bool randomize_spawn_points = false)
        : map_(map)
        , randomize_spawn_points_(randomize_spawn_points)
        , loot_generator_(loot_base_interval, loot_probability, std::move(loot_random_gen))
        , uniform_gen_(std::move(uniform_gen)) {
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

    // Перемещает собак согласно их скорости за dt_seconds, ограничивая их
    // движение дорогами карты, а затем обрабатывает сбор предметов и
    // возвращение их на базу для всех событий столкновений, произошедших
    // за это перемещение (в хронологическом порядке).
    void Move(double dt_seconds);

    const std::deque<LostObject>& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    // Спрашивает у встроенного LootGenerator, сколько трофеев должно
    // появиться на карте спустя time_delta с момента предыдущего вызова, и
    // генерирует их в случайных точках на случайных дорогах карты.
    void GenerateLoot(TimeInterval time_delta);

private:
    Position GenerateStartPosition() const;

    // Определяет события столкновений собак с предметами и базами,
    // произошедшие при перемещении из start_positions в end_positions
    // (индексы соответствуют порядку обхода dogs_), и применяет их эффект:
    // кладёт предметы в рюкзак (если он не полон) либо опустошает рюкзак
    // при достижении базы. Обрабатывает события строго в хронологическом
    // порядке.
    void GatherItems(const std::vector<Position>& start_positions, const std::vector<Position>& end_positions);

    const Map& map_;
    bool randomize_spawn_points_;
    std::deque<Dog> dogs_;
    std::uint64_t next_dog_id_ = 0;

    std::deque<LostObject> lost_objects_;
    std::uint64_t next_loot_id_ = 0;

    loot_gen::LootGenerator loot_generator_;
    UniformRandomGenerator uniform_gen_;
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

    // Настраивает генератор трофеев значениями из конфигурационного файла.
    // period задаётся в миллисекундах, probability - вероятность появления
    // трофея в течение period.
    void SetLootGeneratorConfig(std::chrono::milliseconds period, double probability);

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    std::deque<GameSession> sessions_;
    MapIdToIndex map_id_to_session_index_;
    bool randomize_spawn_points_ = false;

    std::chrono::milliseconds loot_period_{1000};
    double loot_probability_ = 0.0;
};

}
