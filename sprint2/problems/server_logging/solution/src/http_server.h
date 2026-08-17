#pragma once
#include "logger.h"
#include "sdk.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase {
public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run();

protected:
    using HttpRequest = http::request<http::string_body>;

    explicit SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {
        beast::error_code ec;
        if (auto endpoint = stream_.socket().remote_endpoint(ec); !ec) {
            remote_ip_ = endpoint.address().to_string();
        }
    }

    ~SessionBase() = default;

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        const auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - request_start_)
                                           .count();

        std::optional<std::string_view> content_type;
        if (response.count(http::field::content_type)) {
            content_type = response[http::field::content_type];
        }
        server_logging::LogResponseSent(remote_ip_, response_time_ms, response.result_int(), content_type);

        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

        auto self = GetSharedThis();
        http::async_write(stream_, *safe_response,
                          [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                              self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                          });
    }

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;
    std::string remote_ip_;
    std::chrono::steady_clock::time_point request_start_;

    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void Close();
    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);

    virtual void HandleRequest(HttpRequest&& request) = 0;

    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    RequestHandler request_handler_;

    std::shared_ptr<SessionBase> GetSharedThis() override {
        return this->shared_from_this();
    }

    void HandleRequest(HttpRequest&& request) override {
        request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
            self->Write(std::move(response));
        });
    }
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(request_handler)) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void Run() {
        DoAccept();
    }

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;

    void DoAccept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(sys::error_code ec, tcp::socket socket) {
        if (ec) {
            return ReportError(ec, "accept"sv);
        }

        AsyncRunSession(std::move(socket));

        DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}
