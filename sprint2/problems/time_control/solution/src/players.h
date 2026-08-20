#pragma once
#include <deque>
#include <random>
#include <string>
#include <unordered_map>

#include "model.h"
#include "tagged.h"

namespace app {

namespace detail {
struct TokenTag {};
}  // namespace detail

// A 32-hex-digit authentication token.
using Token = util::Tagged<std::string, detail::TokenTag>;

// Generates pseudo-random 32-hex-digit tokens.
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

// A player is the agent through which a user (identified by a token) controls a dog
// within a particular game session.
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

    // The player's id is the id of the dog it controls, unique within its session.
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

class Players {
public:
    // Registers a new player controlling the given dog within the given session,
    // generating a fresh authentication token for it.
    Player& Add(model::Dog& dog, model::GameSession& session);

    const Player* FindByToken(const Token& token) const;

private:
    PlayerTokens token_generator_;
    std::deque<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
};

}  // namespace app
