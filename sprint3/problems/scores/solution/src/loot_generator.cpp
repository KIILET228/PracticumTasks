#include "loot_generator.h"

#include <cmath>

namespace loot_gen {
using namespace std::chrono;

unsigned LootGenerator::Generate(TimeInterval time_delta, unsigned loot_count, unsigned looter_count) {
    time_without_loot_ += time_delta;

    const unsigned loot_shortage = looter_count > loot_count ? looter_count - loot_count : 0u;
    if (loot_shortage == 0) {
        return 0;
    }

    const double time_without_loot = duration<double>(time_without_loot_).count();
    const double base_interval = duration<double>(base_interval_).count();

    const double raw = loot_shortage * (1 - std::pow(1.0 - probability_, time_without_loot / base_interval)) *
                       random_generator_();
    const unsigned generated_loot = static_cast<unsigned>(std::round(raw));

    const double share = static_cast<double>(generated_loot) / static_cast<double>(loot_shortage);
    time_without_loot_ = duration_cast<TimeInterval>(duration<double>(time_without_loot * (1.0 - share)));

    return generated_loot;
}

}  // namespace loot_gen
