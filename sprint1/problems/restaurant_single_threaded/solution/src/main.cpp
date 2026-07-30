#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <syncstream>
#include <unordered_map>
#include <memory>

namespace net = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

class Hamburger {
public:
    [[nodiscard]] bool IsCutletRoasted() const {
        return cutlet_roasted_;
    }
    void SetCutletRoasted() {
        if (IsCutletRoasted()) {
            throw std::logic_error("Cutlet has been roasted already"s);
        }
        cutlet_roasted_ = true;
    }

    [[nodiscard]] bool HasOnion() const {
        return has_onion_;
    }
    void AddOnion() {
        if (IsPacked()) {
            throw std::logic_error("Hamburger has been packed already"s);
        }
        AssureCutletRoasted();
        has_onion_ = true;
    }

    [[nodiscard]] bool IsPacked() const {
        return is_packed_;
    }
    void Pack() {
        AssureCutletRoasted();
        is_packed_ = true;
    }

private:
    void AssureCutletRoasted() const {
        if (!cutlet_roasted_) {
            throw std::logic_error("Bread has not been roasted yet"s);
        }
    }

    bool cutlet_roasted_ = false;
    bool has_onion_ = false;
    bool is_packed_ = false;
};

std::ostream& operator<<(std::ostream& os, const Hamburger& h) {
    return os << "Hamburger: "sv << (h.IsCutletRoasted() ? "roasted cutlet"sv : " raw cutlet"sv)
              << (h.HasOnion() ? ", onion"sv : ""sv)
              << (h.IsPacked() ? ", packed"sv : ", not packed"sv);
}

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        std::osyncstream os{std::cout};
        os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
           << "s] "sv << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, bool with_onion, OrderHandler handler)
        : timer_(io)
        , id_(id)
        , with_onion_(with_onion)
        , handler_(std::move(handler))
        , logger_("Order " + std::to_string(id)) {
    }

    void Start() {
        logger_.LogMessage("Roasting cutlet...");
        timer_.expires_after(100ms);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnCutletRoasted(ec);
        });
    }

private:
    void OnCutletRoasted(sys::error_code ec) {
        if (ec) {
            handler_(ec, id_, nullptr);
            return;
        }

        hamburger_.SetCutletRoasted();
        logger_.LogMessage("Cutlet roasted");

        if (with_onion_) {
            logger_.LogMessage("Adding onion...");
            timer_.expires_after(50ms);
            timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
                self->OnOnionAdded(ec);
            });
        } else {
            PackHamburger();
        }
    }

    void OnOnionAdded(sys::error_code ec) {
        if (ec) {
            handler_(ec, id_, nullptr);
            return;
        }

        hamburger_.AddOnion();
        logger_.LogMessage("Onion added");
        PackHamburger();
    }

    void PackHamburger() {
        logger_.LogMessage("Packing...");
        timer_.expires_after(50ms);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnPacked(ec);
        });
    }

    void OnPacked(sys::error_code ec) {
        if (ec) {
            handler_(ec, id_, nullptr);
            return;
        }

        hamburger_.Pack();
        logger_.LogMessage("Packed");
        handler_(ec, id_, &hamburger_);
    }

    Timer timer_;
    int id_;
    bool with_onion_;
    OrderHandler handler_;
    Logger logger_;
    Hamburger hamburger_;
};

class Restaurant {
public:
    explicit Restaurant(net::io_context& io)
        : io_(io) {
    }

    int MakeHamburger(bool with_onion, OrderHandler handler) {
        const int order_id = ++next_order_id_;
        auto order = std::make_shared<Order>(io_, order_id, with_onion, std::move(handler));
        order->Start();
        return order_id;
    }

private:
    net::io_context& io_;
    int next_order_id_ = 0;
};

int main() {
    net::io_context io;

    Restaurant restaurant{io};

    Logger logger{"main"s};

    struct OrderResult {
        sys::error_code ec;
        Hamburger hamburger;
    };

    std::unordered_map<int, OrderResult> orders;
    auto handle_result = [&orders](sys::error_code ec, int id, Hamburger* h) {
        orders.emplace(id, OrderResult{ec, ec ? Hamburger{} : *h});
    };

    const int id1 = restaurant.MakeHamburger(false, handle_result);
    const int id2 = restaurant.MakeHamburger(true, handle_result);

    assert(orders.empty());
    io.run();

    assert(orders.size() == 2u);
    {
        const auto& o = orders.at(id1);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(!o.hamburger.HasOnion());
    }
    {
        const auto& o = orders.at(id2);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(o.hamburger.HasOnion());
    }
}