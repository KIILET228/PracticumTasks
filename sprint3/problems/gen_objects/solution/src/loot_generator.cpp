#include "loot_generator.h"

namespace loot_gen {

unsigned LootGenerator::Generate(TimeInterval time_delta, unsigned loot_count, unsigned looter_count) {
    time_without_loot_ += time_delta;

    const unsigned loot_shortage = (looter_count > loot_count) ? (looter_count - loot_count) : 0u;

    if (loot_shortage == 0 || base_interval_.count() <= 0) {
        return 0;
    }

    unsigned generated_loot = 0;
    // For every whole base_interval that has elapsed without a successful
    // roll, make one Bernoulli trial with the configured probability.
    while (time_without_loot_ >= base_interval_ && generated_loot < loot_shortage) {
        time_without_loot_ -= base_interval_;
        if (random_generator_() < probability_) {
            ++generated_loot;
        }
    }

    return generated_loot;
}

}  // namespace loot_gen
