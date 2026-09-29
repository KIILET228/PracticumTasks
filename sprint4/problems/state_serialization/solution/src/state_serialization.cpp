#include "state_serialization.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>

#include "model_serialization.h"

namespace state_serialization {

namespace {

namespace ba = boost::archive;

// Снимок состояния одного игрока.
struct PlayerRepr {
    std::string token;
    std::uint32_t dog_id = 0;
    std::string map_id;
    std::int64_t total_time_ms = 0;
    std::int64_t idle_time_ms = 0;
    bool retired = false;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token;
        ar& dog_id;
        ar& map_id;
        ar& total_time_ms;
        ar& idle_time_ms;
        ar& retired;
    }
};

// Снимок состояния одной игровой сессии.
struct SessionRepr {
    std::string map_id;
    std::vector<serialization::DogRepr> dogs;
    std::vector<serialization::LostObjectRepr> lost_objects;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id;
        ar& dogs;
        ar& lost_objects;
    }
};

// Полное состояние игры: сессии + игроки.
struct GameStateRepr {
    std::vector<SessionRepr> sessions;
    std::vector<PlayerRepr> players;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& sessions;
        ar& players;
    }
};

}  // namespace

void SaveState(const app::Application& app, const std::filesystem::path& path) {
    GameStateRepr state;

    // 1. Сохраняем игровые сессии.
    for (const auto& [map_id, session] : app.GetGame().GetSessions()) {
        SessionRepr session_repr;
        session_repr.map_id = *map_id;

        for (const auto& dog : session.GetDogs()) {
            session_repr.dogs.emplace_back(*dog);
        }
        for (const auto& [lost_id, lost] : session.GetLostObjects()) {
            session_repr.lost_objects.emplace_back(lost);
        }

        state.sessions.push_back(std::move(session_repr));
    }

    // 2. Сохраняем игроков (токены, привязка к собаке/сессии, таймеры).
    for (const auto& player : app.GetPlayers().GetAll()) {
        PlayerRepr repr;
        repr.token = *player.GetToken();
        repr.dog_id = *player.GetDog().GetId();
        repr.map_id = *player.GetSession().GetMap().GetId();
        repr.total_time_ms = player.GetTotalTime().count();
        repr.idle_time_ms = player.GetIdleTime().count();
        repr.retired = player.IsRetired();
        state.players.push_back(std::move(repr));
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        throw std::runtime_error("Cannot open state file for writing: " + path.string());
    }
    ba::text_oarchive oa(ofs);
    oa << state;
}

void LoadState(app::Application& app, const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    GameStateRepr state;
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error("Cannot open state file for reading: " + path.string());
        }
        ba::text_iarchive ia(ifs);
        ia >> state;
    }

    // Полностью очищаем игровое состояние.
    app.ClearPlayers();

    auto& game = app.GetGame();
    game.ResetSessions();

    // Восстанавливаем сессии.
    for (const auto& session_repr : state.sessions) {
        const model::Map::Id map_id{session_repr.map_id};
        auto& session = game.JoinSession(map_id);

        for (const auto& dog_repr : session_repr.dogs) {
            session.AddRestoredDog(dog_repr.Restore());
        }
        for (const auto& lost_repr : session_repr.lost_objects) {
            session.AddRestoredLostObject(lost_repr.Restore());
        }
    }

    // Восстанавливаем игроков.
    for (const auto& player_repr : state.players) {
        const model::Map::Id map_id{player_repr.map_id};
        auto& session = game.JoinSession(map_id);
        auto* dog = session.FindDog(model::Dog::Id{player_repr.dog_id});
        if (!dog) {
            continue;  // защита от рассогласованного состояния
        }

        app.RestorePlayer(
            app::Token{player_repr.token},
            *dog,
            session,
            std::chrono::milliseconds{player_repr.total_time_ms},
            std::chrono::milliseconds{player_repr.idle_time_ms},
            player_repr.retired);
    }
}

}  // namespace state_serialization