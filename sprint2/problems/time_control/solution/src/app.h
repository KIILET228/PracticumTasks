#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

#include "model.h"
#include "players.h"

namespace app {

// A game-logic error, identified by a protocol-agnostic error code
// (e.g. "mapNotFound", "invalidArgument", "unknownToken").
// The HTTP layer is responsible for translating this into a status code.
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

// Implements the game's use cases (joining, listing players, ...),
// independent of any particular transport protocol.
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

    // Throws ApplicationError("invalidArgument", ...) if user_name is empty,
    // or ApplicationError("mapNotFound", ...) if the map does not exist.
    JoinGameResult JoinGame(const std::string& user_name, const std::string& map_id_str);

    // Throws ApplicationError("unknownToken", ...) if the token is not recognized.
    std::map<std::uint64_t, PlayerInfo> GetPlayers(const Token& token) const;

    // Throws ApplicationError("unknownToken", ...) if the token is not recognized.
    std::map<std::uint64_t, PlayerState> GetGameState(const Token& token) const;

    // Sets the controlled dog's speed/direction according to move ("L"/"R"/"U"/"D"/"").
    // Throws ApplicationError("unknownToken", ...) if the token is not recognized,
    // or ApplicationError("invalidArgument", "Failed to parse action") if move is not
    // one of the five accepted values.
    void SetPlayerAction(const Token& token, const std::string& move);

    // Advances game time by `delta`, moving every dog in every active session
    // according to its current speed and the road-network rules.
    void Tick(std::chrono::milliseconds delta);

private:
    model::Game& game_;
    Players players_;
};

}  // namespace app
