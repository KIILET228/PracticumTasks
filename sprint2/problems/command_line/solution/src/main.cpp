#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>

#include "http_server.h"
#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<std::chrono::milliseconds> tick_period;
    bool randomize_spawn_points = false;
};

std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc{"Allowed options"s};

    unsigned tick_period_ms = 0;
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&tick_period_ms)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points),
         "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path is not specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files root path is not specified"s);
    }

    if (vm.contains("tick-period"s)) {
        args.tick_period = std::chrono::milliseconds(tick_period_ms);
    }

    return args;
}

}

int main(int argc, const char* argv[]) {
    Args args;
    try {
        auto parsed = ParseCommandLine(argc, argv);
        if (!parsed) {

            return EXIT_SUCCESS;
        }
        args = std::move(*parsed);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    server_logging::InitLogging();

    try {
        model::Game game = json_loader::LoadGame(args.config_file);
        game.SetRandomizeSpawnPoints(args.randomize_spawn_points);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const bool tick_endpoint_enabled = !args.tick_period.has_value();
        http_handler::RequestHandler handler{game, fs::path(args.www_root), ioc, tick_endpoint_enabled};

        if (args.tick_period) {
            handler.EnablePeriodicTicks(*args.tick_period);
        }

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        server_logging::LogServerStarted(port, address.to_string());

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        server_logging::LogServerExited(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }

    server_logging::LogServerExited(0);
}
