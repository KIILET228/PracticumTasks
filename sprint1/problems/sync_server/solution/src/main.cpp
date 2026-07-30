#ifdef WIN32
#include <sdkddkver.h>
#endif
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <string>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

void handle_request(tcp::socket& socket) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        http::response<http::string_body> res;

        if (req.method() == http::verb::get || req.method() == http::verb::head) {
            // Явное преобразование string_view в string
            std::string target(req.target());
            
            // Удаляем ведущий слэш, если он есть
            if (!target.empty() && target[0] == '/') {
                target = target.substr(1);
            }
            
            std::string body = "Hello, " + target;
            res.set(http::field::content_type, "text/html");
            res.content_length(body.size());
            res.body() = body;
            res.result(http::status::ok);
        } else {
            std::string body = "Invalid method.";
            res.set(http::field::content_type, "text/html");
            res.set(http::field::allow, "GET, HEAD");
            res.content_length(body.size());
            res.body() = body;
            res.result(http::status::method_not_allowed);
        }

        res.prepare_payload();

        // Если запрос HEAD, отправляем только заголовки
        if (req.method() == http::verb::head) {
            http::response<http::empty_body> head_res;
            head_res.result(res.result());
            head_res.set(http::field::content_type, res[http::field::content_type]);
            head_res.set(http::field::content_length, res[http::field::content_length]);
            if (res.find(http::field::allow) != res.end()) {
                head_res.set(http::field::allow, res[http::field::allow]);
            }
            head_res.prepare_payload();
            http::write(socket, head_res);
        } else {
            http::write(socket, res);
        }
        
        socket.shutdown(tcp::socket::shutdown_send);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 8080));
        
        std::cout << "Server has started..." << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            
            std::thread([s = std::move(socket)]() mutable {
                handle_request(s);
            }).detach();
        }
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}