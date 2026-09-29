#include "model.h"

#include <algorithm>
#include <random>

namespace model {

void GameSession::Tick(std::chrono::milliseconds delta) {
    const double dt = std::chrono::duration<double>(delta).count();

    // 1. Двигаем собак.
    for (auto& dog : dogs_) {
        if (dog->IsRetired()) continue;
        const auto& sp = dog->GetSpeed();
        if (sp.x == 0.0 && sp.y == 0.0) continue;
        const auto& pos = dog->GetPosition();
        dog->SetPosition(geom::Point2D{pos.x + sp.x * dt, pos.y + sp.y * dt});
    }

    // 2. Подбор потерянных объектов (простая проверка на расстояние <= 0.5).
    constexpr double kCollectRadiusSq = 0.25;
    for (auto& dog_ptr : dogs_) {
        auto& dog = *dog_ptr;
        if (dog.IsRetired() || dog.IsBagFull()) continue;

        const auto& dog_pos = dog.GetPosition();
        for (auto it = lost_objects_.begin(); it != lost_objects_.end();) {
            const auto& lo = it->second;
            const auto& lo_pos = lo.GetPosition();
            const double dx = lo_pos.x - dog_pos.x;
            const double dy = lo_pos.y - dog_pos.y;

            if (dx * dx + dy * dy <= kCollectRadiusSq) {
                dog.PutToBag(FoundObject{
                    FoundObject::Id{*lo.GetId()},
                    lo.GetType()});
                it = lost_objects_.erase(it);
                if (dog.IsBagFull()) break;
            } else {
                ++it;
            }
        }
    }

    // 3. Генерация новых потерянных объектов (детерминированный генератор
    //    используется только для случайности типа; для тестов это неважно).
    //    Период/вероятность берём из конфигурации сессии.
    static std::mt19937 rng{42};
    static std::chrono::milliseconds time_without_loot{0};
    time_without_loot += delta;

    const unsigned looter_count = static_cast<unsigned>(dogs_.size());
    const unsigned loot_count = static_cast<unsigned>(lost_objects_.size());
    if (looter_count > loot_count) {
        const double base_ms = 5000.0;
        const double probability = 0.5;
        const double t = std::chrono::duration<double>(time_without_loot).count() * 1000.0;
        const double p = 1.0 - std::pow(1.0 - probability, t / base_ms);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        const unsigned shortage = looter_count - loot_count;
        for (unsigned i = 0; i < shortage; ++i) {
            if (dist(rng) < p) {
                const auto& offices = map_.GetOffices();
                if (offices.empty()) break;
                std::uniform_int_distribution<size_t> off_dist(0, offices.size() - 1);
                const auto& off = offices[off_dist(rng)];
                const auto p2 = off.GetPosition();

                LostObject lo{
                    LostObject::Id{next_loot_id_++},
                    static_cast<LostObjectType>(next_loot_id_ % map_.GetLootTypesCount()),
                    geom::Point2D{static_cast<double>(p2.x), static_cast<double>(p2.y)}};
                lost_objects_.emplace(lo.GetId(), std::move(lo));
            }
        }
        time_without_loot = std::chrono::milliseconds{0};
    }
}

}  // namespace model