#include "app.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <algorithm>
#include <deque>
#include <fstream>

namespace app {

using namespace std::literals;

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

GameStateResult Application::GetGameState(const Token& token) const {
    const Player* player = players_.FindByToken(token);
    if (!player) {
        throw ApplicationError("unknownToken", "Player token has not been found");
    }

    GameStateResult result;
    for (const auto& dog : player->GetSession().GetDogs()) {
        std::vector<BagItemInfo> bag;
        bag.reserve(dog.GetBag().size());
        for (const auto& bag_item : dog.GetBag()) {
            bag.push_back(BagItemInfo{*bag_item.id, bag_item.type});
        }
        result.players.emplace(
            *dog.GetId(),
            PlayerState{dog.GetPosition(), dog.GetSpeed(), dog.GetDirection(), std::move(bag), dog.GetScore()});
    }
    for (const auto& lost_object : player->GetSession().GetLostObjects()) {
        result.lost_objects.emplace(*lost_object.GetId(),
                                    LostObjectState{lost_object.GetType(), lost_object.GetPosition()});
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

namespace {

// Формат файла (простой самодельный, поверх boost::archive::text_*archive,
// который умеет сохранять примитивы "из коробки" без написания serialize()
// для каждого класса модели):
//
// sessions_count
// для каждой сессии:
//   map_id
//   dogs_count
//   для каждой собаки: id, name, pos.x, pos.y, speed.vx, speed.vy, direction,
//                      score, bag_count, [bag: item_id, item_type]*
//   lost_objects_count
//   для каждого предмета: id, type, pos.x, pos.y
// players_count
// для каждого игрока: token, dog_id, map_id

template <typename Archive>
void SaveDog(Archive& ar, const model::Dog& dog) {
    std::uint64_t id = *dog.GetId();
    ar << id;
    std::string name = dog.GetName();
    ar << name;
    const auto pos = dog.GetPosition();
    ar << pos.x << pos.y;
    const auto speed = dog.GetSpeed();
    ar << speed.vx << speed.vy;
    int direction = static_cast<int>(dog.GetDirection());
    ar << direction;
    unsigned score = dog.GetScore();
    ar << score;

    const auto& bag = dog.GetBag();
    std::uint64_t bag_count = bag.size();
    ar << bag_count;
    for (const auto& item : bag) {
        std::uint64_t item_id = *item.id;
        unsigned item_type = item.type;
        ar << item_id;
        ar << item_type;
    }
}

template <typename Archive>
void SaveLostObject(Archive& ar, const model::LostObject& object) {
    std::uint64_t id = *object.GetId();
    unsigned type = object.GetType();
    const auto pos = object.GetPosition();
    ar << id;
    ar << type;
    ar << pos.x << pos.y;
}

}  // namespace

void Application::SaveState(const std::filesystem::path& path) const {
    const auto tmp_path = path.string() + ".tmp"s;
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to open state file for writing: "s + path.string());
        }
        boost::archive::text_oarchive ar(out);

        std::vector<const model::GameSession*> active_sessions;
        for (const auto& map : game_.GetMaps()) {
            if (const auto* session = game_.FindSession(map.GetId())) {
                active_sessions.push_back(session);
            }
        }

        std::uint64_t sessions_count = active_sessions.size();
        ar << sessions_count;

        for (const auto* session : active_sessions) {
            std::string map_id_str = *session->GetMap().GetId();
            ar << map_id_str;

            const auto& dogs = session->GetDogs();
            std::uint64_t dogs_count = dogs.size();
            ar << dogs_count;
            for (const auto& dog : dogs) {
                SaveDog(ar, dog);
            }

            const auto& lost_objects = session->GetLostObjects();
            std::uint64_t lost_count = lost_objects.size();
            ar << lost_count;
            for (const auto& object : lost_objects) {
                SaveLostObject(ar, object);
            }
        }

        const auto player_records = players_.GetAllForSerialization();
        std::uint64_t players_count = player_records.size();
        ar << players_count;
        for (const auto& record : player_records) {
            std::string token = record.token;
            std::uint64_t dog_id = record.dog_id;
            std::string map_id = record.map_id;
            ar << token;
            ar << dog_id;
            ar << map_id;
        }
    }
    // Атомарно подменяем файл, чтобы падение/убийство процесса посреди
    // записи не могло повредить уже существующий файл состояния.
    std::filesystem::rename(tmp_path, path);
}

void Application::LoadState(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return;
    }
    boost::archive::text_iarchive ar(in);

    std::uint64_t sessions_count = 0;
    ar >> sessions_count;

    for (std::uint64_t i = 0; i < sessions_count; ++i) {
        std::string map_id_str;
        ar >> map_id_str;
        model::GameSession& session = game_.JoinSession(model::Map::Id{map_id_str});

        std::uint64_t dogs_count = 0;
        ar >> dogs_count;
        for (std::uint64_t d = 0; d < dogs_count; ++d) {
            std::uint64_t id = 0;
            ar >> id;
            std::string name;
            ar >> name;
            model::Position pos{};
            ar >> pos.x >> pos.y;
            model::Speed speed{};
            ar >> speed.vx >> speed.vy;
            int direction_int = 0;
            ar >> direction_int;
            unsigned score = 0;
            ar >> score;

            std::uint64_t bag_count = 0;
            ar >> bag_count;
            std::vector<model::BagItem> bag;
            bag.reserve(bag_count);
            for (std::uint64_t b = 0; b < bag_count; ++b) {
                std::uint64_t item_id = 0;
                unsigned item_type = 0;
                ar >> item_id;
                ar >> item_type;
                bag.push_back(model::BagItem{model::LostObject::Id{item_id}, item_type});
            }

            session.RestoreDog(model::Dog::Id{id}, name, pos, speed, static_cast<model::Direction>(direction_int),
                               score, bag);
        }

        std::uint64_t lost_count = 0;
        ar >> lost_count;
        std::deque<model::LostObject> lost_objects;
        for (std::uint64_t l = 0; l < lost_count; ++l) {
            std::uint64_t id = 0;
            unsigned type = 0;
            double x = 0.0;
            double y = 0.0;
            ar >> id;
            ar >> type;
            ar >> x >> y;
            lost_objects.emplace_back(model::LostObject::Id{id}, type, model::Position{x, y});
        }
        session.RestoreLostObjects(std::move(lost_objects));
    }

    std::uint64_t players_count = 0;
    ar >> players_count;
    for (std::uint64_t i = 0; i < players_count; ++i) {
        std::string token_str;
        std::uint64_t dog_id = 0;
        std::string map_id_str;
        ar >> token_str;
        ar >> dog_id;
        ar >> map_id_str;

        model::GameSession& session = game_.JoinSession(model::Map::Id{map_id_str});
        model::Dog& dog = session.GetDogById(model::Dog::Id{dog_id});
        players_.AddWithToken(Token{token_str}, dog, session);
    }
}

}
