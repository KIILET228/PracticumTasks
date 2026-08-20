#pragma once
#include <boost/beast/http.hpp>
#include <string_view>

#include "app.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

// Handles every request whose target starts with "/api/".
class ApiHandler {
public:
    explicit ApiHandler(model::Game& game)
        : application_{game} {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    StringResponse HandleApiRequest(const StringRequest& req);

private:
    app::Application application_;

    StringResponse HandleJoin(const StringRequest& req);
    StringResponse HandlePlayers(const StringRequest& req) const;
    StringResponse HandleState(const StringRequest& req) const;
    StringResponse HandleMapsApi(http::verb method, std::string_view target, unsigned version,
                                 bool keep_alive) const;
};

}  // namespace http_handler
