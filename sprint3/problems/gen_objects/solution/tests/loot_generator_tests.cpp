#include <catch2/catch_test_macros.hpp>

#include <deque>

#include "../src/loot_generator.h"

using namespace std::chrono_literals;
using loot_gen::LootGenerator;

namespace {

// Returns a generator that always claims the roll succeeded (returns 0.0,
// which is < any positive probability).
LootGenerator::RandomGenerator AlwaysSucceed() {
    return [] {
        return 0.0;
    };
}

// Returns a generator that always claims the roll failed (returns a value
// that is never less than any probability in [0, 1]).
LootGenerator::RandomGenerator AlwaysFail() {
    return [] {
        return 1.0;
    };
}

// Returns values from a fixed sequence, one per call, repeating the last
// value once exhausted.
LootGenerator::RandomGenerator FromSequence(std::deque<double> values) {
    auto seq = std::make_shared<std::deque<double>>(std::move(values));
    return [seq] {
        if (seq->empty()) {
            return 1.0;
        }
        const double value = seq->front();
        if (seq->size() > 1) {
            seq->pop_front();
        }
        return value;
    };
}

}  // namespace

TEST_CASE("No loot is generated when there is no shortage", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    // loot_count >= looter_count, so nothing should be generated regardless
    // of how much time passes or what the RNG says.
    CHECK(gen.Generate(5000ms, 3, 3) == 0);
    CHECK(gen.Generate(5000ms, 5, 3) == 0);
}

TEST_CASE("No loot is generated before a full base interval has elapsed", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    // Even though there's a shortage and every roll would succeed, less
    // than one full base_interval has elapsed, so nothing appears yet.
    CHECK(gen.Generate(500ms, 0, 3) == 0);
    CHECK(gen.Generate(400ms, 0, 3) == 0);
    // The accumulated 900ms is still not a full 1000ms interval.
}

TEST_CASE("Exactly one loot item appears once a base interval elapses and the roll succeeds",
         "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    CHECK(gen.Generate(1000ms, 0, 3) == 1);
}

TEST_CASE("No loot appears when the roll fails, even after a full interval", "[LootGenerator]") {
    LootGenerator gen{1000ms, 0.5, AlwaysFail()};

    CHECK(gen.Generate(1000ms, 0, 3) == 0);
    CHECK(gen.Generate(1000ms, 0, 3) == 0);
}

TEST_CASE("Generated loot never exceeds the shortage of loot relative to looters", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    // 10 whole intervals elapse in one call, but only 2 items are missing.
    const auto generated = gen.Generate(10000ms, 1, 3);
    CHECK(generated == 2);
}

TEST_CASE("Multiple whole intervals in a single call can each produce loot", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    // 3 whole intervals elapse; shortage is large enough not to cap the count.
    CHECK(gen.Generate(3000ms, 0, 10) == 3);
}

TEST_CASE("Leftover time carries over between calls", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    // 600ms + 600ms = 1200ms, i.e. one full interval elapsed in total.
    CHECK(gen.Generate(600ms, 0, 3) == 0);
    CHECK(gen.Generate(600ms, 0, 3) == 1);
}

TEST_CASE("Each elapsed interval gets its own independent roll", "[LootGenerator]") {
    // Rolls: succeed, fail, succeed -> 2 out of 3 intervals produce loot.
    LootGenerator gen{1000ms, 0.5, FromSequence({0.0, 0.9, 0.1})};

    CHECK(gen.Generate(3000ms, 0, 10) == 2);
}

TEST_CASE("A non-positive base interval never generates loot", "[LootGenerator]") {
    LootGenerator gen{0ms, 1.0, AlwaysSucceed()};

    CHECK(gen.Generate(10000ms, 0, 5) == 0);
}

TEST_CASE("Zero elapsed time never generates loot on its own", "[LootGenerator]") {
    LootGenerator gen{1000ms, 1.0, AlwaysSucceed()};

    CHECK(gen.Generate(0ms, 0, 5) == 0);
}

TEST_CASE("The default random generator never produces loot", "[LootGenerator]") {
    // The default generator always returns 1.0, which is never < a
    // probability in [0, 1], so loot never appears unless a real random
    // source is explicitly supplied.
    LootGenerator gen{1000ms, 1.0};

    CHECK(gen.Generate(10000ms, 0, 5) == 0);
}
