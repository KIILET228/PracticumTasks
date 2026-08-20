#include "api_handler.h"

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <optional>

namespace http_handler {

namespace json = boost::json;
using namespace std::literals;

namespace {

constexpr std::string_view kGameJoin = "/api/v1/game/join"sv;
constexpr std::string_view kGamePlayers = "/api/v1/game/players"sv;
constexpr std::string_view kGameState = "/api/v1/game/state"sv;
constexpr std::string_view kMapsApi = "/api/v1/maps"sv;
constexpr std::string_view kMapsApiPrefix = "/api/v1/maps/"sv;
constexpr boost::beast::string_view kContentTypeJson = "application/json";

StringResponse MakeJsonResponse(http::status status, const json::value& value, unsigned version, bool keep_alive,
                                bool include_body) {
    std::string body = json::serialize(value);

    StringResponse response(status, version);
    response.set(http::field::content_type, kContentTypeJson);
    response.set(http::field::cache_control, "no-cache");
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    if (include_body) {
        response.body() = std::move(body);
    }
    return response;
}

StringResponse MakeErrorResponse(http::status status, std::string_view code, std::string_view message,
                                 unsigned version, bool keep_alive, bool include_body) {
    json::object error;
    error["code"] = std::string(code);
    error["message"] = std::string(message);
    return MakeJsonResponse(status, error, version, keep_alive, include_body);
}

json::value RoadToJson(const model::Road& road) {
    json::object obj;
    const auto start = road.GetStart();
    obj["x0"] = start.x;
    obj["y0"] = start.y;
    if (road.IsHorizontal()) {
        obj["x1"] = road.GetEnd().x;
    } else {
        obj["y1"] = road.GetEnd().y;
    }
    return obj;
}

json::value BuildingToJson(const model::Building& building) {
    const auto& bounds = building.GetBounds();
    json::object obj;
    obj["x"] = bounds.position.x;
    obj["y"] = bounds.position.y;
    obj["w"] = bounds.size.width;
    obj["h"] = bounds.size.height;
    return obj;
}

json::value OfficeToJson(const model::Office& office) {
    const auto position = office.GetPosition();
    const auto offset = office.GetOffset();
    json::object obj;
    obj["id"] = *office.GetId();
    obj["x"] = position.x;
    obj["y"] = position.y;
    obj["offsetX"] = offset.dx;
    obj["offsetY"] = offset.dy;
    return obj;
}

json::value MapToBriefJson(const model::Map& map) {
    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    return obj;
}

json::value MapToFullJson(const model::Map& map) {
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        roads.push_back(RoadToJson(road));
    }
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        buildings.push_back(BuildingToJson(building));
    }
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        offices.push_back(OfficeToJson(office));
    }

    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = std::move(roads);
    obj["buildings"] = std::move(buildings);
    obj["offices"] = std::move(offices);
    return obj;
}

std::string_view DirectionToString(model::Direction dir) {
    switch (dir) {
        case model::Direction::NORTH:
            return "U"sv;
        case model::Direction::SOUTH:
            return "D"sv;
        case model::Direction::WEST:
            return "L"sv;
        case model::Direction::EAST:
            return "R"sv;
    }
    return "U"sv;
}

// Extracts and validates the bearer token from the Authorization header.
// Returns std::nullopt if the header is missing or its value is malformed.
std::optional<app::Token> TryExtractToken(const StringRequest& req) {
    const auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return std::nullopt;
    }

    const std::string_view value(it->value().data(), it->value().size());
    constexpr std::string_view kBearerPrefix = "Bearer "sv;
    constexpr size_t kTokenLength = 32;

    if (value.size() != kBearerPrefix.size() + kTokenLength || !value.starts_with(kBearerPrefix)) {
        return std::nullopt;
    }

    const std::string_view token_sv = value.substr(kBearerPrefix.size());
    if (!std::all_of(token_sv.begin(), token_sv.end(), [](unsigned char c) {
            return std::isxdigit(c);
        })) {
        return std::nullopt;
    }

    std::string token_str{token_sv};
    std::transform(token_str.begin(), token_str.end(), token_str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return app::Token{std::move(token_str)};
}

}  // namespace

StringResponse ApiHandler::HandleApiRequest(const StringRequest& req) {
    const auto target_bsv = req.target();
    const std::string_view target(target_bsv.data(), target_bsv.size());

    if (target == kGameJoin) {
        return HandleJoin(req);
    }
    if (target == kGamePlayers) {
        return HandlePlayers(req);
    }
    if (target == kGameState) {
        return HandleState(req);
    }
    return HandleMapsApi(req.method(), target, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandleJoin(const StringRequest& req) {
    const unsigned version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::post) {
        StringResponse response = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod"sv,
                                                     "Only POST method is expected"sv, version, keep_alive, true);
        response.set(http::field::allow, "POST");
        return response;
    }

    std::string user_name;
    std::string map_id;
    try {
        const auto value = json::parse(req.body());
        const auto& obj = value.as_object();
        user_name = std::string(obj.at("userName").as_string());
        map_id = std::string(obj.at("mapId").as_string());
    } catch (const std::exception&) {
        return MakeErrorResponse(http::status::bad_request, "invalidArgument"sv, "Join game request parse error"sv,
                                 version, keep_alive, true);
    }

    try {
        const auto result = application_.JoinGame(user_name, map_id);
        json::object body;
        body["authToken"] = *result.token;
        body["playerId"] = result.player_id;
        return MakeJsonResponse(http::status::ok, body, version, keep_alive, true);
    } catch (const app::ApplicationError& err) {
        const auto status = (err.GetCode() == "mapNotFound") ? http::status::not_found : http::status::bad_request;
        return MakeErrorResponse(status, err.GetCode(), err.what(), version, keep_alive, true);
    }
}

StringResponse ApiHandler::HandlePlayers(const StringRequest& req) const {
    const unsigned version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        StringResponse response = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod"sv,
                                                     "Invalid method"sv, version, keep_alive, true);
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    const bool include_body = (req.method() != http::verb::head);

    const auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken"sv, "Authorization header is missing"sv,
                                 version, keep_alive, include_body);
    }

    try {
        const auto players = application_.GetPlayers(*token);
        json::object body;
        for (const auto& [id, info] : players) {
            json::object player_obj;
            player_obj["name"] = info.name;
            body[std::to_string(id)] = std::move(player_obj);
        }
        return MakeJsonResponse(http::status::ok, body, version, keep_alive, include_body);
    } catch (const app::ApplicationError& err) {
        return MakeErrorResponse(http::status::unauthorized, err.GetCode(), err.what(), version, keep_alive,
                                 include_body);
    }
}

StringResponse ApiHandler::HandleState(const StringRequest& req) const {
    const unsigned version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        StringResponse response = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod"sv,
                                                     "Invalid method"sv, version, keep_alive, true);
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    const bool include_body = (req.method() != http::verb::head);

    const auto token = TryExtractToken(req);
    if (!token) {
        return MakeErrorResponse(http::status::unauthorized, "invalidToken"sv, "Authorization header is required"sv,
                                 version, keep_alive, include_body);
    }

    try {
        const auto players = application_.GetGameState(*token);
        json::object players_json;
        for (const auto& [id, state] : players) {
            json::array pos;
            pos.push_back(state.pos.x);
            pos.push_back(state.pos.y);

            json::array speed;
            speed.push_back(state.speed.vx);
            speed.push_back(state.speed.vy);

            json::object player_obj;
            player_obj["pos"] = std::move(pos);
            player_obj["speed"] = std::move(speed);
            player_obj["dir"] = std::string(DirectionToString(state.dir));
            players_json[std::to_string(id)] = std::move(player_obj);
        }
        json::object body;
        body["players"] = std::move(players_json);
        return MakeJsonResponse(http::status::ok, body, version, keep_alive, include_body);
    } catch (const app::ApplicationError& err) {
        return MakeErrorResponse(http::status::unauthorized, err.GetCode(), err.what(), version, keep_alive,
                                 include_body);
    }
}

StringResponse ApiHandler::HandleMapsApi(http::verb method, std::string_view target, unsigned version,
                                         bool keep_alive) const {
    const bool include_body = (method != http::verb::head);
    const bool method_supported = (method == http::verb::get || method == http::verb::head);

    if (target == kMapsApi) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, "invalidMethod"sv, "Invalid method"sv,
                                     version, keep_alive, include_body);
        }
        json::array maps_json;
        for (const auto& map : application_.GetGame().GetMaps()) {
            maps_json.push_back(MapToBriefJson(map));
        }
        return MakeJsonResponse(http::status::ok, maps_json, version, keep_alive, include_body);
    }

    if (target.starts_with(kMapsApiPrefix)) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, "invalidMethod"sv, "Invalid method"sv,
                                     version, keep_alive, include_body);
        }
        const std::string map_id_str{target.substr(kMapsApiPrefix.size())};
        const model::Map::Id map_id{map_id_str};
        if (const auto* map = application_.GetGame().FindMap(map_id)) {
            return MakeJsonResponse(http::status::ok, MapToFullJson(*map), version, keep_alive, include_body);
        }
        return MakeErrorResponse(http::status::not_found, "mapNotFound"sv, "Map not found"sv, version, keep_alive,
                                 include_body);
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest"sv, "Bad request"sv, version, keep_alive,
                             include_body);
}

}  // namespace http_handler
