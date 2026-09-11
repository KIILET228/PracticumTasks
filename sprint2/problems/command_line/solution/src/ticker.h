#pragma once
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <functional>
#include <memory>

namespace net = boost::asio;

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{std::move(strand)}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->last_tick_time_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    using Clock = std::chrono::steady_clock;

    void ScheduleTick() {
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](boost::system::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(boost::system::error_code ec) {
        if (ec) {
            return;
        }

        const auto this_tick_time = Clock::now();
        const auto delta =
            std::chrono::duration_cast<std::chrono::milliseconds>(this_tick_time - last_tick_time_);
        last_tick_time_ = this_tick_time;

        handler_(delta);

        ScheduleTick();
    }

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    Clock::time_point last_tick_time_;
};
