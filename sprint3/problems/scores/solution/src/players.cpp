#include "players.h"

#include <iomanip>
#include <sstream>

namespace app {

Token PlayerTokens::GenerateToken() {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << generator1_();
    oss << std::hex << std::setfill('0') << std::setw(16) << generator2_();
    return Token{oss.str()};
}

Player& Players::Add(model::Dog& dog, model::GameSession& session) {
    Token token = token_generator_.GenerateToken();
    Player& player = players_.emplace_back(std::move(token), dog, session);
    token_to_player_.emplace(player.GetToken(), &player);
    return player;
}

const Player* Players::FindByToken(const Token& token) const {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

}
