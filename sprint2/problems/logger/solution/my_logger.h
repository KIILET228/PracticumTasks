#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return oss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Выведите в поток все аргументы.
    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Определяем, в какой файл нужно писать лог на текущий момент.
        // Дата берётся из GetTime()/manual_ts_, так что при смене даты
        // (в том числе "искусственной", через SetTimestamp) мы откроем
        // другой файл.
        const std::string file_ts = GetFileTimeStamp();
        if (!log_file_.is_open() || file_ts != current_file_ts_) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_file_ts_ = file_ts;
            log_file_.open("/var/log/sample_log_"s + current_file_ts_ + ".log"s,
                            std::ios::app);
        }

        log_file_ << GetTimeStamp() << ": "sv;
        (log_file_ << ... << args);
        log_file_ << std::endl;
    }

    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;

    std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_file_ts_;
};
