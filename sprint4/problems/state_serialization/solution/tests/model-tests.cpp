#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/model.h"

using namespace std::chrono_literals;
using Catch::Matchers::WithinAbs;

namespace {

model::Map MakeTestMap(unsigned loot_types_count) {
    model::Map map{model::Map::Id{"map1"}, "Test map"};
    // Two roads: a horizontal one from (0,0) to (10,0), and a vertical one
    // from (10,0) to (10,10).
    map.AddRoad(model::Road{model::Road::HORIZONTAL, model::Point{0, 0}, 10});
    map.AddRoad(model::Road{model::Road::VERTICAL, model::Point{10, 0}, 10});
    map.SetLootTypesCount(loot_types_count);
    return map;
}

// A random source that always returns the same fixed value in [0, 1).
model::GameSession::UniformRandomGenerator FixedValue(double value) {
    return [value] {
        return value;
    };
}

// LootGenerator::Generate multiplies its exponential-decay probability
// estimate by whatever random_generator_() returns (a noise factor in
// [0, 1]); returning 1.0 applies no dampening, i.e. the full computed
// shortage is filled as soon as at least one base_interval has elapsed.
loot_gen::LootGenerator::RandomGenerator AlwaysSucceed() {
    return [] {
        return 1.0;
    };
}

}  // namespace

TEST_CASE("A freshly parsed map exposes its loot type count", "[Model]") {
    const auto map = MakeTestMap(3);
    CHECK(map.GetLootTypesCount() == 3);
}

TEST_CASE("No loot is generated for a session with no dogs", "[GameSession]") {
    const auto map = MakeTestMap(2);
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.0)};

    // looter_count == 0, so even though the roll always succeeds, there's
    // never a shortage to fill.
    session.GenerateLoot(10000ms);

    CHECK(session.GetLostObjects().empty());
}

TEST_CASE("Loot generation never exceeds the number of dogs in the session", "[GameSession]") {
    const auto map = MakeTestMap(2);
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.0)};

    session.AddDog("Rex");
    session.AddDog("Fido");

    // Plenty of elapsed time and an always-succeeding roll: still capped at
    // the number of dogs (2).
    session.GenerateLoot(10000ms);
    CHECK(session.GetLostObjects().size() == 2);

    // Further ticks shouldn't add more once the shortage is filled.
    session.GenerateLoot(10000ms);
    CHECK(session.GetLostObjects().size() == 2);
}

TEST_CASE("Generated lost objects land exactly on the chosen road", "[GameSession]") {
    const auto map = MakeTestMap(5);
    // uniform_gen_() is called in this order per object: road selection,
    // point-along-road (t), type selection. With a fixed generator returning
    // 0.0, that always resolves to road index 0, t = 0 (the road's start),
    // and type 0.
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.0)};
    session.AddDog("Rex");

    session.GenerateLoot(1000ms);

    REQUIRE(session.GetLostObjects().size() == 1);
    const auto& object = session.GetLostObjects().front();
    CHECK(object.GetType() == 0);
    CHECK_THAT(object.GetPosition().x, WithinAbs(0.0, 1e-9));
    CHECK_THAT(object.GetPosition().y, WithinAbs(0.0, 1e-9));
}

TEST_CASE("A uniform value near 1 selects the last road and its far endpoint", "[GameSession]") {
    const auto map = MakeTestMap(5);
    // 0.999... selects the last road (index 1: the vertical one from (10,0)
    // to (10,10)) and a point very close to its end, as well as the last
    // loot type.
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.999)};
    session.AddDog("Rex");

    session.GenerateLoot(1000ms);

    REQUIRE(session.GetLostObjects().size() == 1);
    const auto& object = session.GetLostObjects().front();
    CHECK(object.GetType() == 4);
    CHECK_THAT(object.GetPosition().x, WithinAbs(10.0, 1e-9));
    CHECK_THAT(object.GetPosition().y, WithinAbs(9.99, 1e-9));
}

TEST_CASE("Loot type is always within [0, lootTypesCount)", "[GameSession]") {
    const auto map = MakeTestMap(1);
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.5)};
    session.AddDog("Rex");

    session.GenerateLoot(1000ms);

    REQUIRE(session.GetLostObjects().size() == 1);
    // With only one loot type configured, the type must always be 0, even
    // though the raw uniform value (0.5) would suggest otherwise for a
    // larger type count.
    CHECK(session.GetLostObjects().front().GetType() == 0);
}

TEST_CASE("Lost object ids are unique and increase monotonically", "[GameSession]") {
    const auto map = MakeTestMap(2);
    model::GameSession session{map, 1000ms, 1.0, AlwaysSucceed(), FixedValue(0.0)};
    session.AddDog("Rex");
    session.AddDog("Fido");
    session.AddDog("Buddy");

    session.GenerateLoot(3000ms);

    REQUIRE(session.GetLostObjects().size() == 3);
    std::uint64_t expected_id = 0;
    for (const auto& object : session.GetLostObjects()) {
        CHECK(*object.GetId() == expected_id);
        ++expected_id;
    }
}
