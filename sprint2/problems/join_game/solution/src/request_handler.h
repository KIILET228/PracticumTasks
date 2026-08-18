#pragma once
#include "api_handler.h"
#include "http_server.h"
#include "model.h"

#include <filesystem>
#include <string_view>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

using FileResponse = http::response<http::file_body>;

class RequestHandler {
public:
    RequestHandler(model::Game& game, fs::path static_root)
        : api_handler_{game}
        , static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Send>
    void operator()(StringRequest&& req, Send&& send) {
        const auto target = req.target();
        const std::string_view target_sv(target.data(), target.size());

        if (target_sv.starts_with(kApiPrefix)) {
            send(api_handler_.HandleApiRequest(req));
            return;
        }

        auto response = HandleFileRequest(req.method(), target_sv, req.version(), req.keep_alive());
        std::visit([&send](auto&& resp) {
            send(std::forward<decltype(resp)>(resp));
        }, std::move(response));
    }

private:
    static constexpr std::string_view kApiPrefix = "/api/";

    ApiHandler api_handler_;
    fs::path static_root_;

    std::variant<StringResponse, FileResponse> HandleFileRequest(http::verb method, std::string_view target,
                                                                  unsigned version, bool keep_alive) const;
};

}
