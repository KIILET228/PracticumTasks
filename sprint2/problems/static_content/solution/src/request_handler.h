#pragma once
#include "http_server.h"
#include "model.h"

#include <filesystem>
#include <string_view>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

class RequestHandler {
public:
    RequestHandler(model::Game& game, fs::path static_root)
        : game_{game}
        , static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto target = req.target();
        const std::string_view target_sv(target.data(), target.size());

        if (target_sv.starts_with(kApiPrefix)) {
            send(HandleApiRequest(req.method(), target_sv, req.version(), req.keep_alive()));
            return;
        }

        auto response = HandleFileRequest(req.method(), target_sv, req.version(), req.keep_alive());
        std::visit([&send](auto&& resp) {
            send(std::forward<decltype(resp)>(resp));
        }, std::move(response));
    }

private:
    static constexpr std::string_view kApiPrefix = "/api/";

    model::Game& game_;
    fs::path static_root_;

    StringResponse HandleApiRequest(http::verb method, std::string_view target, unsigned version,
                                    bool keep_alive) const;

    std::variant<StringResponse, FileResponse> HandleFileRequest(http::verb method, std::string_view target,
                                                                  unsigned version, bool keep_alive) const;
};

}
