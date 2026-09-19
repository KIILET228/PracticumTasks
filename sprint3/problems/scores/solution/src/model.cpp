#include "model.h"

#include "collision_detector.h"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

constexpr double kRoadHalfWidth = 0.4;
constexpr double kEps = 1e-9;

// Половина ширины собаки (0.6) и базы (0.5) - используются для проверки
// коллизий: собиратель (собака) сталкивается с предметом/базой, если
// расстояние между ними не превышает суммы их половин ширины.
constexpr double kDogHalfWidth = 0.3;
constexpr double kOfficeHalfWidth = 0.25;

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

// Мост между моделью игры и обобщённой функцией поиска коллизий
// collision_detector::FindGatherEvents. "Предметами" здесь считаются как
// потерянные вещи на карте (индексы [0, lost_objects.size())), так и базы
// (индексы [lost_objects.size(), lost_objects.size() + offices.size())).
class ItemAndOfficeProvider : public collision_detector::ItemGathererProvider {
public:
    ItemAndOfficeProvider(const std::deque<LostObject>& lost_objects, const Map::Offices& offices,
                          const std::vector<Position>& start_positions, const std::vector<Position>& end_positions)
        : lost_objects_(lost_objects)
        , offices_(offices)
        , start_positions_(start_positions)
        , end_positions_(end_positions) {
    }

    size_t ItemsCount() const override {
        return lost_objects_.size() + offices_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        if (idx < lost_objects_.size()) {
            const Position pos = lost_objects_[idx].GetPosition();
            // Ширина самих предметов равна нулю.
            return collision_detector::Item{{pos.x, pos.y}, 0.0};
        }
        const Point pos = offices_[idx - lost_objects_.size()].GetPosition();
        return collision_detector::Item{{static_cast<double>(pos.x), static_cast<double>(pos.y)}, kOfficeHalfWidth};
    }

    size_t GatherersCount() const override {
        return start_positions_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return collision_detector::Gatherer{{start_positions_[idx].x, start_positions_[idx].y},
                                            {end_positions_[idx].x, end_positions_[idx].y}, kDogHalfWidth};
    }

private:
    const std::deque<LostObject>& lost_objects_;
    const Map::Offices& offices_;
    const std::vector<Position>& start_positions_;
    const std::vector<Position>& end_positions_;
};

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

void GameSession::GatherItems(const std::vector<Position>& start_positions,
                              const std::vector<Position>& end_positions) {
    if (dogs_.empty()) {
        return;
    }

    const size_t items_count = lost_objects_.size();

    ItemAndOfficeProvider provider{lost_objects_, map_.GetOffices(), start_positions, end_positions};
    const auto events = collision_detector::FindGatherEvents(provider);

    // Отмечает предметы, которые уже забрал кто-то из собак в этом тике -
    // чтобы тот же предмет не забрал второй раз кто-то ещё.
    std::vector<bool> item_taken(items_count, false);

    for (const auto& event : events) {
        Dog& dog = dogs_[event.gatherer_id];

        if (event.item_id < items_count) {
            // Столкновение с потерянным предметом.
            if (item_taken[event.item_id] || dog.IsBagFull()) {
                continue;
            }
            const LostObject& object = lost_objects_[event.item_id];
            dog.TryPutInBag(object.GetId(), object.GetType());
            item_taken[event.item_id] = true;
        } else {
            // Столкновение с базой - начисляем очки за предметы в рюкзаке
            // и сдаём их все.
            unsigned points = 0;
            for (const auto& bag_item : dog.GetBag()) {
                points += map_.GetLootValue(bag_item.type);
            }
            dog.AddScore(points);
            dog.ClearBag();
        }
    }

    if (items_count == 0) {
        return;
    }

    size_t index = 0;
    std::erase_if(lost_objects_, [&item_taken, &index](const LostObject&) {
        return item_taken[index++];
    });
}

void GameSession::Move(double dt_seconds) {
    std::vector<Position> start_positions;
    std::vector<Position> end_positions;
    start_positions.reserve(dogs_.size());
    end_positions.reserve(dogs_.size());

    for (Dog& dog : dogs_) {
        const Position start_pos = dog.GetPosition();
        Position result = start_pos;

        const Speed speed = dog.GetSpeed();
        if (speed.vx != 0.0 || speed.vy != 0.0) {
            const Position unclamped{start_pos.x + speed.vx * dt_seconds, start_pos.y + speed.vy * dt_seconds};
            const Bounds bounds = map_.ComputeBounds(start_pos);

            result = unclamped;
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

        start_positions.push_back(start_pos);
        end_positions.push_back(dog.GetPosition());
    }

    GatherItems(start_positions, end_positions);
}

Dog& GameSession::AddDog(std::string name) {
    Dog::Id id{next_dog_id_++};
    return dogs_.emplace_back(id, std::move(name), GenerateStartPosition(), map_.GetBagCapacity());
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
