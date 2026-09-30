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
    std::optional<std::string> state_file;
    std::optional<std::chrono::milliseconds> save_state_period;
};

std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc{"Allowed options"s};

    unsigned tick_period_ms = 0;
    std::string state_file;
    unsigned save_state_period_ms = 0;
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&tick_period_ms)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points),
         "spawn dogs at random positions")
        ("state-file", po::value(&state_file)->value_name("file"s), "set game state file path")
        ("save-state-period", po::value(&save_state_period_ms)->value_name("milliseconds"s),
         "set game state autosave period");

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

    if (vm.contains("state-file"s)) {
        args.state_file = state_file;
    }

    if (vm.contains("save-state-period"s)) {
        if (!args.state_file) {
            throw std::runtime_error("--save-state-period requires --state-file to be set"s);
        }
        args.save_state_period = std::chrono::milliseconds(save_state_period_ms);
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
        auto game_data = json_loader::LoadGame(args.config_file);
        game_data.game.SetRandomizeSpawnPoints(args.randomize_spawn_points);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        const bool tick_endpoint_enabled = !args.tick_period.has_value();
        http_handler::RequestHandler handler{game_data.game, game_data.loot_types_info, fs::path(args.www_root), ioc,
                                             tick_endpoint_enabled};

        // Если указан файл состояния и он существует - восстанавливаем игру
        // из него. Делаем это до того, как io_context начнёт обрабатывать
        // запросы (ioc.run() ещё не вызван), поэтому конкурентного доступа
        // к данным быть не может.
        if (args.state_file && fs::exists(*args.state_file)) {
            handler.GetApplication().LoadState(*args.state_file);
        }

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &handler, &args](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (ec) {
                return;
            }
            // Сохранение состояния должно быть строго упорядочено
            // относительно обработки API-запросов и игровых тиков (все они
            // выполняются на api_strand_), иначе возможна гонка данных в
            // многопоточном io_context. Поэтому не сохраняем состояние прямо
            // здесь, а ставим это в очередь того же strand'а и останавливаем
            // io_context уже после того, как сохранение гарантированно
            // выполнено.
            handler.PostOnApiStrand([&ioc, &handler, &args] {
                if (args.state_file) {
                    try {
                        handler.SaveState(*args.state_file);
                    } catch (const std::exception& save_ex) {
                        server_logging::LogError(0, save_ex.what(), "SaveStateOnSignal"sv);
                    }
                }
                ioc.stop();
            });
        });

        if (args.tick_period) {
            handler.EnablePeriodicTicks(*args.tick_period);
        }

        if (args.state_file && args.save_state_period) {
            handler.EnablePeriodicStateSaving(*args.save_state_period, *args.state_file);
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
