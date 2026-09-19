#include "app.h"

namespace app {

JoinGameResult Application::JoinGame(const std::string& user_name, const std::string& map_id_str) {
    if (user_name.empty()) {
        throw ApplicationError("invalidArgument", "Invalid name");
    }

    const model::Map::Id map_id{map_id_str};
    if (!game_.FindMap(map_id)) {
        throw ApplicationError("mapNotFound", "Map not found");
    }

    model::GameSession& session = game_.JoinSession(map_id);
    model::Dog& dog = session.AddDog(user_name);
    Player& player = players_.Add(dog, session);

    return JoinGameResult{player.GetToken(), player.GetId()};
}

std::map<std::uint64_t, PlayerInfo> Application::GetPlayers(const Token& token) const {
    const Player* player = players_.FindByToken(token);
    if (!player) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    std::map<std::uint64_t, PlayerInfo> result;
    for (const auto& dog : player->GetSession().GetDogs()) {
        result.emplace(*dog.GetId(), PlayerInfo{dog.GetName()});
    }
    return result;
}

std::map<std::uint64_t, PlayerState> Application::GetGameState(const Token& token) const {
    const Player* player = players_.FindByToken(token);
    if (!player) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    std::map<std::uint64_t, PlayerState> result;
    for (const auto& dog : player->GetSession().GetDogs()) {
        result.emplace(*dog.GetId(), PlayerState{dog.GetPosition(), dog.GetSpeed(), dog.GetDirection()});
    }
    return result;
}

void Application::SetPlayerAction(const Token& token, const std::string& move) {
    const Player* player = players_.FindByToken(token);
    if (!player) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    const double speed = player->GetSession().GetMap().GetDogSpeed();
    model::Dog& dog = player->GetDog();

    if (move == "L") {
        dog.SetSpeed(model::Speed{-speed, 0.0});
        dog.SetDirection(model::Direction::WEST);
    } else if (move == "R") {
        dog.SetSpeed(model::Speed{speed, 0.0});
        dog.SetDirection(model::Direction::EAST);
    } else if (move == "U") {
        dog.SetSpeed(model::Speed{0.0, -speed});
        dog.SetDirection(model::Direction::NORTH);
    } else if (move == "D") {
        dog.SetSpeed(model::Speed{0.0, speed});
        dog.SetDirection(model::Direction::SOUTH);
    } else if (move.empty()) {

        dog.SetSpeed(model::Speed{0.0, 0.0});
    } else {
        throw ApplicationError("invalidArgument", "Failed to parse action");
    }
}

void Application::Tick(std::chrono::milliseconds delta) {
    game_.Tick(delta);
}

}
