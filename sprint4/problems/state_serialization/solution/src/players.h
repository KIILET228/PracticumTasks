#pragma once
#include <chrono>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>

#include "model.h"
#include "tagged.h"

namespace app {

namespace detail {
struct TokenTag {};
}

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    Token GenerateToken();

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

class Player {
public:
    Player(Token token, model::Dog& dog, model::GameSession& session) noexcept
        : token_(std::move(token))
        , dog_(dog)
        , session_(session) {
    }

    const Token& GetToken() const noexcept { return token_; }
    std::uint64_t GetId() const noexcept { return *dog_.GetId(); }
    model::Dog& GetDog() const noexcept { return dog_; }
    model::GameSession& GetSession() const noexcept { return session_; }

    void Tick(std::chrono::milliseconds delta) noexcept {
        total_time_ += delta;
        idle_time_ += delta;
    }
    void ResetIdle() noexcept { idle_time_ = std::chrono::milliseconds::zero(); }

    std::chrono::milliseconds GetIdleTime() const noexcept { return idle_time_; }
    std::chrono::milliseconds GetTotalTime() const noexcept { return total_time_; }

    bool IsRetired() const noexcept { return retired_; }
    void Retire() noexcept {
        retired_ = true;
        dog_.SetSpeed(model::Speed{0.0, 0.0});
        dog_.SetRetired();
    }

    // Используется при восстановлении состояния из файла.
    void RestoreTimers(std::chrono::milliseconds total_time,
                       std::chrono::milliseconds idle_time,
                       bool retired) noexcept {
        total_time_ = total_time;
        idle_time_ = idle_time;
        retired_ = retired;
    }

private:
    Token token_;
    model::Dog& dog_;
    model::GameSession& session_;
    std::chrono::milliseconds total_time_{};
    std::chrono::milliseconds idle_time_{};
    bool retired_ = false;
};

class Players {
public:
    Player& Add(model::Dog& dog, model::GameSession& session);

    // Добавляет игрока с уже известными token и таймерами (для загрузки состояния).
    Player& AddRestored(Token token,
                        model::Dog& dog,
                        model::GameSession& session,
                        std::chrono::milliseconds total_time,
                        std::chrono::milliseconds idle_time,
                        bool retired);

    const Player* FindByToken(const Token& token) const;
    Player* FindByTokenMutable(const Token& token);

    std::deque<Player>& GetAll() noexcept { return players_; }
    const std::deque<Player>& GetAll() const noexcept { return players_; }

    // Полная очистка (для загрузки состояния).
    void Clear() noexcept {
        players_.clear();
        token_to_player_.clear();
    }

private:
    PlayerTokens token_generator_;
    std::deque<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
};

}  // namespace app