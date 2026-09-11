#include "http_server.h"

namespace http_server {

void ReportError(beast::error_code ec, std::string_view what) {
    server_logging::LogError(ec.value(), ec.message(), what);
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(), [self = GetSharedThis()] {
        self->Read();
    });
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
                     [self = GetSharedThis()](beast::error_code ec, std::size_t bytes_read) {
                         self->OnRead(ec, bytes_read);
                     });
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }

    request_start_ = std::chrono::steady_clock::now();
    const auto target = request_.target();
    const auto method = request_.method_string();
    server_logging::LogRequestReceived(remote_ip_, std::string_view(target.data(), target.size()),
                                       std::string_view(method.data(), method.size()));

    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        ReportError(ec, "shutdown"sv);
    }
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        return Close();
    }

    Read();
}

}
