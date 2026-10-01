#pragma once
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

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

    const Token& GetToken() const noexcept {
        return token_;
    }

    std::uint64_t GetId() const noexcept {
        return *dog_.GetId();
    }

    model::Dog& GetDog() const noexcept {
        return dog_;
    }

    model::GameSession& GetSession() const noexcept {
        return session_;
    }

private:
    Token token_;
    model::Dog& dog_;
    model::GameSession& session_;
};

// Запись об игроке в формате, пригодном для сохранения на диск: явно хранит
// токен (строкой), id собаки и id карты, к которой относится сессия.
struct PlayerRecordForSave {
    std::string token;
    std::uint64_t dog_id = 0;
    std::string map_id;
};

class Players {
public:

    Player& Add(model::Dog& dog, model::GameSession& session);

    // Добавляет игрока с уже существующим (например, восстановленным из
    // файла) токеном, а не сгенерированным заново.
    Player& AddWithToken(Token token, model::Dog& dog, model::GameSession& session);

    const Player* FindByToken(const Token& token) const;

    // Возвращает записи обо всех игроках в виде, пригодном для сохранения.
    std::vector<PlayerRecordForSave> GetAllForSerialization() const;

private:
    PlayerTokens token_generator_;
    std::deque<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
};

}
