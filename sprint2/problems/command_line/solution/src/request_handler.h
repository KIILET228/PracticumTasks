#pragma once
#include "api_handler.h"
#include "http_server.h"
#include "model.h"
#include "ticker.h"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string_view>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace fs = std::filesystem;

using FileResponse = http::response<http::file_body>;

class RequestHandler {
public:

    RequestHandler(model::Game& game, fs::path static_root, net::io_context& ioc, bool tick_endpoint_enabled)
        : api_handler_{game, tick_endpoint_enabled}
        , static_root_{fs::weakly_canonical(static_root)}
        , api_strand_{net::make_strand(ioc)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void EnablePeriodicTicks(std::chrono::milliseconds period) {
        std::make_shared<Ticker>(api_strand_, period, [this](std::chrono::milliseconds delta) {
            api_handler_.Tick(delta);
        })->Start();
    }

    template <typename Send>
    void operator()(StringRequest&& req, Send&& send) {
        const auto target = req.target();
        const std::string_view target_sv(target.data(), target.size());

        if (target_sv.starts_with(kApiPrefix)) {

            net::dispatch(api_strand_, [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                send(api_handler_.HandleApiRequest(req));
            });
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
    net::strand<net::io_context::executor_type> api_strand_;

    std::variant<StringResponse, FileResponse> HandleFileRequest(http::verb method, std::string_view target,
                                                                  unsigned version, bool keep_alive) const;
};

}
