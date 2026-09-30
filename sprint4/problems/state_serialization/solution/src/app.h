#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
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

// Предмет в рюкзаке игрока, как он должен попасть в ответ на
// /api/v1/game/state. id и type - те же, что были у предмета до подбора.
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

    GameStateResult GetGameState(const Token& token) const;

    void SetPlayerAction(const Token& token, const std::string& move);

    void Tick(std::chrono::milliseconds delta);

    // Сохраняет полное состояние игры (собаки, потерянные вещи, токены
    // игроков) в файл по указанному пути. Файл перезаписывается целиком.
    void SaveState(const std::filesystem::path& path) const;

    // Загружает состояние игры из файла, ранее сохранённого SaveState.
    // Если файл не существует, ничего не делает (игра остаётся пустой).
    // Должна вызываться до начала обработки запросов, когда в игре ещё нет
    // ни одной сессии/игрока.
    void LoadState(const std::filesystem::path& path);

private:
    model::Game& game_;
    Players players_;
};

}
