#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kEps = 1e-10;

// Простая реализация ItemGathererProvider поверх двух векторов -
// ровно то, о чём говорится в задании: "объект класса можно заменить
// парой векторов, но выбрано более универсальное решение".
class TestProvider : public collision_detector::ItemGathererProvider {
public:
    TestProvider(std::vector<collision_detector::Item> items, std::vector<collision_detector::Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

bool IsSortedByTime(const std::vector<collision_detector::GatheringEvent>& events) {
    return std::is_sorted(events.begin(), events.end(),
                          [](const auto& l, const auto& r) {
                              return l.time < r.time;
                          });
}

// Ищет среди событий такое, которое относится к заданным индексам предмета
// и собирателя. Пригодится, чтобы не зависеть от порядка одновременных
// событий (задание явно разрешает любой порядок для одновременных событий).
const collision_detector::GatheringEvent* FindEvent(const std::vector<collision_detector::GatheringEvent>& events,
                                                     size_t item_id, size_t gatherer_id) {
    auto it = std::find_if(events.begin(), events.end(), [&](const auto& e) {
        return e.item_id == item_id && e.gatherer_id == gatherer_id;
    });
    return it == events.end() ? nullptr : &*it;
}

}  // namespace

SCENARIO("FindGatherEvents handles the absence of items or gatherers", "[FindGatherEvents]") {
    GIVEN("no items and no gatherers at all") {
        TestProvider provider{{}, {}};
        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("nothing is found") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("some items but no gatherers") {
        TestProvider provider{{collision_detector::Item{{0.0, 0.0}, 1.0}}, {}};
        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("nothing is found, since nobody moves") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("some gatherers but no items") {
        TestProvider provider{{}, {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};
        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("nothing is found, since there is nothing to collect") {
                CHECK(events.empty());
            }
        }
    }
}

SCENARIO("A gatherer that hasn't moved never collects anything", "[FindGatherEvents]") {
    GIVEN("a gatherer whose start and end positions coincide, right on top of an item") {
        TestProvider provider{{collision_detector::Item{{5.0, 0.0}, 10.0}},
                              {collision_detector::Gatherer{{5.0, 0.0}, {5.0, 0.0}, 10.0}}};
        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("no event is generated, no matter how close the item is") {
                CHECK(events.empty());
            }
        }
    }
}

SCENARIO("A single gatherer collecting a single item", "[FindGatherEvents]") {
    GIVEN("a gatherer moving along the X axis and an item exactly on its path") {
        // Собиратель идёт из (0,0) в (10,0), предмет лежит прямо на пути в точке (4,0).
        TestProvider provider{{collision_detector::Item{{4.0, 0.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("exactly one event is found") {
                REQUIRE(events.size() == 1);
            }

            THEN("the event refers to the only item and the only gatherer") {
                CHECK(events.front().item_id == 0);
                CHECK(events.front().gatherer_id == 0);
            }

            THEN("the squared distance is zero, since the item lies exactly on the path") {
                CHECK_THAT(events.front().sq_distance, WithinAbs(0.0, kEps));
            }

            THEN("the collection happens 40% of the way through the movement") {
                CHECK_THAT(events.front().time, WithinAbs(0.4, kEps));
            }
        }
    }

    GIVEN("an item that lies exactly on the boundary of the combined collection radius") {
        // Собиратель радиуса 0.5 идёт из (0,0) в (10,0).
        // Предмет радиуса 0.5 находится в точке (5, 1.0) - расстояние до прямой
        // движения равно ровно 1.0 = 0.5 + 0.5, то есть точно на границе.
        TestProvider provider{{collision_detector::Item{{5.0, 1.0}, 0.5}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("the boundary case (<=) counts as a collection") {
                REQUIRE(events.size() == 1);
                CHECK_THAT(events.front().sq_distance, WithinAbs(1.0, kEps));
                CHECK_THAT(events.front().time, WithinAbs(0.5, kEps));
            }
        }
    }

    GIVEN("an item that lies just outside the combined collection radius") {
        TestProvider provider{{collision_detector::Item{{5.0, 1.0 + 1e-3}, 0.5}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("no event is generated") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("an item that lies before the start of the movement segment") {
        // Проекция предмета на прямую движения находится левее старта (ratio < 0),
        // хотя по расстоянию предмет мог бы быть собран.
        TestProvider provider{{collision_detector::Item{{-1.0, 0.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("it is not collected, since the projection falls outside the segment") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("an item that lies right after the end of the movement segment") {
        TestProvider provider{{collision_detector::Item{{11.0, 0.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("it is not collected, since the projection falls outside the segment") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("an item far away from the gatherer's path") {
        TestProvider provider{{collision_detector::Item{{5.0, 100.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);
            THEN("it is not collected") {
                CHECK(events.empty());
            }
        }
    }
}

SCENARIO("Multiple items collected by a single gatherer are reported in order", "[FindGatherEvents]") {
    GIVEN("three items scattered along the gatherer's path, given out of order") {
        // Собиратель идёт из (0,0) в (10,0), радиус 1.
        // Предметы (в специально перемешанном порядке индексов):
        //   idx 0: (8,0) -> time 0.8
        //   idx 1: (2,0) -> time 0.2
        //   idx 2: (5,0) -> time 0.5
        TestProvider provider{{collision_detector::Item{{8.0, 0.0}, 1.0}, collision_detector::Item{{2.0, 0.0}, 1.0},
                               collision_detector::Item{{5.0, 0.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("all three items are collected") {
                REQUIRE(events.size() == 3);
            }

            THEN("events come out in chronological order") {
                REQUIRE(IsSortedByTime(events));
                CHECK_THAT(events[0].time, WithinAbs(0.2, kEps));
                CHECK_THAT(events[1].time, WithinAbs(0.5, kEps));
                CHECK_THAT(events[2].time, WithinAbs(0.8, kEps));
            }

            THEN("each event still refers to the correct original item index") {
                CHECK(events[0].item_id == 1);
                CHECK(events[1].item_id == 2);
                CHECK(events[2].item_id == 0);
            }
        }
    }

    GIVEN("an item that is outside the path and one that is inside it") {
        TestProvider provider{{collision_detector::Item{{5.0, 0.0}, 1.0},
                               collision_detector::Item{{5.0, 1000.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("only the reachable item produces an event") {
                REQUIRE(events.size() == 1);
                CHECK(events.front().item_id == 0);
            }
        }
    }
}

SCENARIO("An item can be collected by several different gatherers", "[FindGatherEvents]") {
    GIVEN("two gatherers whose paths both cross the same item") {
        // Собиратель 0: (0,0) -> (10,0), собирает предмет в момент 0.5.
        // Собиратель 1: (0,10) -> (0,-10) (по вертикали через ту же точку),
        // собирает тот же предмет в момент 0.5.
        TestProvider provider{{collision_detector::Item{{5.0, 0.0}, 1.0}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0},
                               collision_detector::Gatherer{{5.0, 10.0}, {5.0, -10.0}, 1.0}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("the item is reported as collected twice - once per gatherer") {
                REQUIRE(events.size() == 2);
            }

            THEN("both events reference the same item but different gatherers") {
                const auto* event_from_first = FindEvent(events, /*item_id=*/0, /*gatherer_id=*/0);
                const auto* event_from_second = FindEvent(events, /*item_id=*/0, /*gatherer_id=*/1);
                REQUIRE(event_from_first != nullptr);
                REQUIRE(event_from_second != nullptr);
                CHECK_THAT(event_from_first->time, WithinAbs(0.5, kEps));
                CHECK_THAT(event_from_second->time, WithinAbs(0.5, kEps));
            }
        }
    }
}

SCENARIO("A complex scene with several gatherers and items is handled correctly", "[FindGatherEvents]") {
    GIVEN("two gatherers and four items, only some of which are actually collected") {
        // Собиратель 0: (0,0) -> (10,0), радиус 0.5.
        //   Предмет A (idx 0): (2,0)   радиус 0.5 -> собран, time=0.2
        //   Предмет B (idx 1): (8,5)   радиус 0.5 -> НЕ собран (слишком далеко)
        // Собиратель 1: (0,0) -> (0,10), радиус 0.5.
        //   Предмет C (idx 2): (0,7)   радиус 0.5 -> собран, time=0.7
        //   Предмет D (idx 3): (-5,-1) радиус 0.5 -> НЕ собран ни одним собирателем
        //                       (позади старта у обоих, вне досягаемости)
        TestProvider provider{{collision_detector::Item{{2.0, 0.0}, 0.5}, collision_detector::Item{{8.0, 5.0}, 0.5},
                               collision_detector::Item{{0.0, 7.0}, 0.5}, collision_detector::Item{{-5.0, -1.0}, 0.5}},
                              {collision_detector::Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5},
                               collision_detector::Gatherer{{0.0, 0.0}, {0.0, 10.0}, 0.5}}};

        WHEN("looking for gathering events") {
            const auto events = collision_detector::FindGatherEvents(provider);

            THEN("exactly the two reachable items produce events") {
                REQUIRE(events.size() == 2);
            }

            THEN("no extra, non-existent collisions are reported") {
                CHECK(FindEvent(events, /*item_id=*/1, /*gatherer_id=*/0) == nullptr);
                CHECK(FindEvent(events, /*item_id=*/3, /*gatherer_id=*/1) == nullptr);
            }

            THEN("the found events have correct indices and timing") {
                const auto* a = FindEvent(events, /*item_id=*/0, /*gatherer_id=*/0);
                const auto* c = FindEvent(events, /*item_id=*/2, /*gatherer_id=*/1);
                REQUIRE(a != nullptr);
                REQUIRE(c != nullptr);
                CHECK_THAT(a->time, WithinAbs(0.2, kEps));
                CHECK_THAT(a->sq_distance, WithinAbs(0.0, kEps));
                CHECK_THAT(c->time, WithinAbs(0.7, kEps));
                CHECK_THAT(c->sq_distance, WithinAbs(0.0, kEps));
            }

            THEN("events are still reported in chronological order") {
                CHECK(IsSortedByTime(events));
            }
        }
    }
}
