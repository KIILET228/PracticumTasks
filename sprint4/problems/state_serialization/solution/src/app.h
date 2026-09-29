#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "model.h"
#include "players.h"

namespace app {

class ApplicationError : public std::runtime_error {
public:
    ApplicationError(std::string code, std::string message)
        : std::runtime_error(std::move(message))
        , code_(std::move(code)) {
    }

    const std::string& GetCode() const noexcept {
        return code_;
    }

private:
    std::string code_;
};

struct JoinGameResult {
    Token token;
    std::uint64_t player_id;
};

struct PlayerInfo {
    std::string name;
};

struct BagItemInfo {
    std::uint64_t id;
    unsigned type;
};

struct PlayerState {
    model::Position pos;
    model::Speed speed;
    model::Direction dir;
    std::vector<BagItemInfo> bag;
    unsigned score;
};

struct LostObjectState {
    unsigned type;
    model::Position pos;
};

struct GameStateResult {
    std::map<std::uint64_t, PlayerState> players;
    std::map<std::uint64_t, LostObjectState> lost_objects;
};

struct PlayerRecord {
    std::string name;
    unsigned score;
    double play_time_seconds;
};

class Application {
public:
    explicit Application(model::Game& game) noexcept
        : game_(game) {
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    model::Game& GetGame() const noexcept {
        return game_;
    }

    // Доступ к игрокам для сериализации состояния.
    const Players& GetPlayers() const noexcept { return players_; }
    Players& GetPlayers() noexcept { return players_; }

    JoinGameResult JoinGame(const std::string& user_name, const std::string& map_id_str);

    std::map<std::uint64_t, PlayerInfo> GetPlayers(const Token& token) const;

    GameStateResult GetGameState(const Token& token) const;

    void SetPlayerAction(const Token& token, const std::string& move);

    void Tick(std::chrono::milliseconds delta);

    std::vector<PlayerRecord> GetRecords(size_t start, size_t max_items) const;

    // === Восстановление состояния ===

    // Полностью очищает список игроков (используется при загрузке состояния).
    void ClearPlayers() noexcept {
        players_.Clear();
    }

    // Восстанавливает игрока из ранее сохранённого состояния.
    void RestorePlayer(Token token,
                       model::Dog& dog,
                       model::GameSession& session,
                       std::chrono::milliseconds total_time,
                       std::chrono::milliseconds idle_time,
                       bool retired);

private:
    model::Game& game_;
    Players players_;
    std::vector<PlayerRecord> records_;
};

}  // namespace app