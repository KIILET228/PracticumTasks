#pragma once
#include <boost/beast/http.hpp>
#include <chrono>
#include <string_view>

#include "app.h"
#include "extra_data.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

// 1

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

class ApiHandler {
public:

    ApiHandler(model::Game& game, const extra_data::LootTypesInfo& loot_types_info, bool tick_endpoint_enabled)
        : application_{game}
        , loot_types_info_{loot_types_info}
        , tick_endpoint_enabled_{tick_endpoint_enabled} {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    StringResponse HandleApiRequest(const StringRequest& req);

    void Tick(std::chrono::milliseconds delta);

private:
    app::Application application_;
    const extra_data::LootTypesInfo& loot_types_info_;
    bool tick_endpoint_enabled_;

    StringResponse HandleJoin(const StringRequest& req);
    StringResponse HandlePlayers(const StringRequest& req) const;
    StringResponse HandleState(const StringRequest& req) const;
    StringResponse HandleAction(const StringRequest& req);
    StringResponse HandleTick(const StringRequest& req);
    StringResponse HandleMapsApi(http::verb method, std::string_view target, unsigned version,
                                 bool keep_alive) const;
};

}
