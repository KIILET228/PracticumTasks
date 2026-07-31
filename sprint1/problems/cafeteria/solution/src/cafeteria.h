#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Вспомогательный класс для управления одним заказом хот-дога
class HotDogOrder : public std::enable_shared_from_this<HotDogOrder> {
public:
    HotDogOrder(net::io_context& io, std::shared_ptr<GasCooker> cooker, Store& store, int id, HotDogHandler handler)
        : timer_(io)
        , cooker_(std::move(cooker))
        , store_(store)
        , id_(id)
        , handler_(std::move(handler))
        , bread_(store_.GetBread())
        , sausage_(store_.GetSausage()) {
    }

    void Start() {
        // Начинаем печь булку
        bread_->StartBake(*cooker_, [self = shared_from_this()] {
            self->OnBreadStarted();
        });

        // Начинаем жарить сосиску
        sausage_->StartFry(*cooker_, [self = shared_from_this()] {
            self->OnSausageStarted();
        });
    }

private:
    void OnBreadStarted() {
        // Булка печется 1 секунду
        timer_.expires_after(Milliseconds{1000});
        timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
            if (ec) return;
            self->bread_->StopBaking();
            self->CheckCompletion();
        });
    }

    void OnSausageStarted() {
        // Сосиска жарится 1.5 секунды
        timer_.expires_after(Milliseconds{1500});
        timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
            if (ec) return;
            self->sausage_->StopFry();
            self->CheckCompletion();
        });
    }

    void CheckCompletion() {
        // Хот-дог готов, когда готовы оба ингредиента
        if (bread_->IsCooked() && sausage_->IsCooked()) {
            try {
                HotDog hot_dog(id_, sausage_, bread_);
                handler_(Result<HotDog>{std::move(hot_dog)});
            } catch (...) {
                handler_(Result<HotDog>::FromCurrentException());
            }
        }
    }

    net::steady_timer timer_;
    std::shared_ptr<GasCooker> cooker_;
    Store& store_;
    int id_;
    HotDogHandler handler_;
    std::shared_ptr<Bread> bread_;
    std::shared_ptr<Sausage> sausage_;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        const int order_id = ++next_order_id_;
        auto order = std::make_shared<HotDogOrder>(io_, gas_cooker_, store_, order_id, std::move(handler));
        order->Start();
    }

private:
    net::io_context& io_;
    int next_order_id_ = 0;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};