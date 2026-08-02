// 1
#include "request_handler.h"

#include <boost/json.hpp>

namespace http_handler {

namespace json = boost::json;
using namespace std::literals;

namespace {

constexpr std::string_view kApiPrefix = "/api/"sv;
constexpr std::string_view kMapsApi = "/api/v1/maps"sv;
constexpr std::string_view kMapsApiPrefix = "/api/v1/maps/"sv;
constexpr boost::beast::string_view kContentTypeJson = "application/json";

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

StringResponse MakeJsonResponse(http::status status, const json::value& value, unsigned version,
                                bool keep_alive, bool include_body) {
    std::string body = json::serialize(value);

    StringResponse response(status, version);
    response.set(http::field::content_type, kContentTypeJson);
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

}

StringResponse RequestHandler::HandleRequest(http::verb method, std::string_view target, unsigned version,
                                             bool keep_alive) const {
    const bool include_body = (method != http::verb::head);
    const bool method_supported = (method == http::verb::get || method == http::verb::head);

    if (target == kMapsApi) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, "badRequest"sv, "Bad request"sv, version,
                                     keep_alive, include_body);
        }
        json::array maps_json;
        for (const auto& map : game_.GetMaps()) {
            maps_json.push_back(MapToBriefJson(map));
        }
        return MakeJsonResponse(http::status::ok, maps_json, version, keep_alive, include_body);
    }

    if (target.starts_with(kMapsApiPrefix)) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, "badRequest"sv, "Bad request"sv, version,
                                     keep_alive, include_body);
        }
        const std::string map_id_str{target.substr(kMapsApiPrefix.size())};
        const model::Map::Id map_id{map_id_str};
        if (const auto* map = game_.FindMap(map_id)) {
            return MakeJsonResponse(http::status::ok, MapToFullJson(*map), version, keep_alive, include_body);
        }
        return MakeErrorResponse(http::status::not_found, "mapNotFound"sv, "Map not found"sv, version, keep_alive,
                                 include_body);
    }

    if (target.starts_with(kApiPrefix)) {
        return MakeErrorResponse(http::status::bad_request, "badRequest"sv, "Bad request"sv, version, keep_alive,
                                 include_body);
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest"sv, "Bad request"sv, version, keep_alive,
                             include_body);
}

}
