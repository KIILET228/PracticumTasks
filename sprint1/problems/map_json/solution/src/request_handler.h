#pragma once
#include "http_server.h"
#include "model.h"

#include <string_view>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        send(HandleRequest(req.method(), req.target(), req.version(), req.keep_alive()));
    }

private:
    model::Game& game_;

    // Не зависит от типа Body/Allocator, поэтому реализация вынесена в .cpp
    StringResponse HandleRequest(http::verb method, std::string_view target, unsigned version,
                                 bool keep_alive) const;
};

}  // namespace http_handler
