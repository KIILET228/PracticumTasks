#pragma once
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

private:
    model::Game& game_;
    Players players_;
};

}  // namespace app
