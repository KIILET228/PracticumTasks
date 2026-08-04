// 1
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
        const auto target = req.target();
        send(HandleRequest(req.method(), std::string_view(target.data(), target.size()),
                           req.version(), req.keep_alive()));
    }

private:
    model::Game& game_;

    StringResponse HandleRequest(http::verb method, std::string_view target, unsigned version,
                                 bool keep_alive) const;
};

}
