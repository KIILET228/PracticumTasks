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

namespace field {
constexpr std::string_view kX0 = "x0"sv;
constexpr std::string_view kY0 = "y0"sv;
constexpr std::string_view kX1 = "x1"sv;
constexpr std::string_view kY1 = "y1"sv;
constexpr std::string_view kX = "x"sv;
constexpr std::string_view kY = "y"sv;
constexpr std::string_view kWidth = "w"sv;
constexpr std::string_view kHeight = "h"sv;
constexpr std::string_view kId = "id"sv;
constexpr std::string_view kOffsetX = "offsetX"sv;
constexpr std::string_view kOffsetY = "offsetY"sv;
constexpr std::string_view kName = "name"sv;
constexpr std::string_view kRoads = "roads"sv;
constexpr std::string_view kBuildings = "buildings"sv;
constexpr std::string_view kOffices = "offices"sv;
}

namespace error {
constexpr std::string_view kBadRequestCode = "badRequest"sv;
constexpr std::string_view kBadRequestMessage = "Bad request"sv;
constexpr std::string_view kMapNotFoundCode = "mapNotFound"sv;
constexpr std::string_view kMapNotFoundMessage = "Map not found"sv;
}

json::value RoadToJson(const model::Road& road) {
    json::object obj;
    const auto start = road.GetStart();
    obj[field::kX0] = start.x;
    obj[field::kY0] = start.y;
    if (road.IsHorizontal()) {
        obj[field::kX1] = road.GetEnd().x;
    } else {
        obj[field::kY1] = road.GetEnd().y;
    }
    return obj;
}

json::value BuildingToJson(const model::Building& building) {
    const auto& bounds = building.GetBounds();
    json::object obj;
    obj[field::kX] = bounds.position.x;
    obj[field::kY] = bounds.position.y;
    obj[field::kWidth] = bounds.size.width;
    obj[field::kHeight] = bounds.size.height;
    return obj;
}

json::value OfficeToJson(const model::Office& office) {
    const auto position = office.GetPosition();
    const auto offset = office.GetOffset();
    json::object obj;
    obj[field::kId] = *office.GetId();
    obj[field::kX] = position.x;
    obj[field::kY] = position.y;
    obj[field::kOffsetX] = offset.dx;
    obj[field::kOffsetY] = offset.dy;
    return obj;
}

json::value MapToBriefJson(const model::Map& map) {
    json::object obj;
    obj[field::kId] = *map.GetId();
    obj[field::kName] = map.GetName();
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
    obj[field::kId] = *map.GetId();
    obj[field::kName] = map.GetName();
    obj[field::kRoads] = std::move(roads);
    obj[field::kBuildings] = std::move(buildings);
    obj[field::kOffices] = std::move(offices);
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
    json::object error_obj;
    error_obj["code"] = std::string(code);
    error_obj["message"] = std::string(message);
    return MakeJsonResponse(status, error_obj, version, keep_alive, include_body);
}

}

StringResponse RequestHandler::HandleRequest(http::verb method, std::string_view target, unsigned version,
                                             bool keep_alive) const {
    const bool include_body = (method != http::verb::head);
    const bool method_supported = (method == http::verb::get || method == http::verb::head);

    if (target == kMapsApi) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, error::kBadRequestCode,
                                     error::kBadRequestMessage, version, keep_alive, include_body);
        }
        json::array maps_json;
        for (const auto& map : game_.GetMaps()) {
            maps_json.push_back(MapToBriefJson(map));
        }
        return MakeJsonResponse(http::status::ok, maps_json, version, keep_alive, include_body);
    }

    if (target.starts_with(kMapsApiPrefix)) {
        if (!method_supported) {
            return MakeErrorResponse(http::status::method_not_allowed, error::kBadRequestCode,
                                     error::kBadRequestMessage, version, keep_alive, include_body);
        }
        const std::string map_id_str{target.substr(kMapsApiPrefix.size())};
        const model::Map::Id map_id{map_id_str};
        if (const auto* map = game_.FindMap(map_id)) {
            return MakeJsonResponse(http::status::ok, MapToFullJson(*map), version, keep_alive, include_body);
        }
        return MakeErrorResponse(http::status::not_found, error::kMapNotFoundCode, error::kMapNotFoundMessage,
                                 version, keep_alive, include_body);
    }

    if (target.starts_with(kApiPrefix)) {
        return MakeErrorResponse(http::status::bad_request, error::kBadRequestCode, error::kBadRequestMessage,
                                 version, keep_alive, include_body);
    }

    return MakeErrorResponse(http::status::bad_request, error::kBadRequestCode, error::kBadRequestMessage,
                             version, keep_alive, include_body);
}

}
