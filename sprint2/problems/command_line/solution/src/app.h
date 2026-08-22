#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

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

struct PlayerState {
    model::Position pos;
    model::Speed speed;
    model::Direction dir;
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

    JoinGameResult JoinGame(const std::string& user_name, const std::string& map_id_str);

    std::map<std::uint64_t, PlayerInfo> GetPlayers(const Token& token) const;

    std::map<std::uint64_t, PlayerState> GetGameState(const Token& token) const;

    void SetPlayerAction(const Token& token, const std::string& move);

    void Tick(std::chrono::milliseconds delta);

private:
    model::Game& game_;
    Players players_;
};

}
