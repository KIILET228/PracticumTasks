#include "app.h"

#include <algorithm>

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
    if (!player || player->IsRetired()) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    std::map<std::uint64_t, PlayerInfo> result;
    for (const auto& dog : player->GetSession().GetDogs()) {
        // dog — это std::shared_ptr<model::Dog>, обращаемся через ->
        result.emplace(*dog->GetId(), PlayerInfo{dog->GetName()});
    }
    return result;
}

GameStateResult Application::GetGameState(const Token& token) const {
    const Player* player = players_.FindByToken(token);
    if (!player || player->IsRetired()) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    GameStateResult result;

    for (const auto& dog : player->GetSession().GetDogs()) {
        std::vector<BagItemInfo> bag;
        bag.reserve(dog->GetBag().size());
        for (const auto& bag_item : dog->GetBag()) {
            bag.push_back(BagItemInfo{*bag_item.id, bag_item.type});
        }

        result.players.emplace(
            *dog->GetId(),
            PlayerState{
                dog->GetPosition(),
                dog->GetSpeed(),
                dog->GetDirection(),
                std::move(bag),
                dog->GetScore()});
    }

    for (const auto& [lost_id, lost_object] : player->GetSession().GetLostObjects()) {
        result.lost_objects.emplace(
            *lost_object.GetId(),
            LostObjectState{lost_object.GetType(), lost_object.GetPosition()});
    }

    return result;
}

void Application::SetPlayerAction(const Token& token, const std::string& move) {
    Player* player = players_.FindByTokenMutable(token);
    if (!player) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    if (player->IsRetired()) {
        if (move.empty()) {
            return;
        }
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

    if (!move.empty()) {
        player->ResetIdle();
    }
}

void Application::Tick(std::chrono::milliseconds delta) {
    game_.Tick(delta);

    const auto retirement_time = game_.GetDogRetirementTime();
    if (retirement_time.count() <= 0) {
        return;
    }

    std::vector<Player*> to_retire;
    for (auto& player : players_.GetAll()) {
        if (player.IsRetired()) {
            continue;
        }
        player.Tick(delta);
        if (player.GetIdleTime() >= retirement_time) {
            to_retire.push_back(&player);
        }
    }

    for (Player* player : to_retire) {
        const auto& dog = player->GetDog();
        records_.push_back(PlayerRecord{
            dog.GetName(),
            dog.GetScore(),
            std::chrono::duration<double>(player->GetTotalTime()).count()});
        player->Retire();
    }
}

std::vector<PlayerRecord> Application::GetRecords(size_t start, size_t max_items) const {
    std::vector<PlayerRecord> sorted = records_;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const PlayerRecord& a, const PlayerRecord& b) {
                         return a.score > b.score;
                     });

    if (start >= sorted.size()) {
        return {};
    }
    const size_t end = std::min(start + max_items, sorted.size());
    return {sorted.begin() + start, sorted.begin() + end};
}

void Application::RestorePlayer(Token token,
                                model::Dog& dog,
                                model::GameSession& session,
                                std::chrono::milliseconds total_time,
                                std::chrono::milliseconds idle_time,
                                bool retired) {
    players_.AddRestored(std::move(token), dog, session, total_time, idle_time, retired);
}

}  // namespace app