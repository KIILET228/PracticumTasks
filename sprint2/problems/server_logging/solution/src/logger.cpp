#include "logger.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <iostream>
#include <utility>

namespace server_logging {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace json = boost::json;

// Attributes attached to every log record we format.
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

namespace {

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object entry;

    if (auto ts = rec[timestamp]) {
        entry["timestamp"] = to_iso_extended_string(*ts);
    }

    if (auto data = rec[additional_data]) {
        entry["data"] = *data;
    } else {
        entry["data"] = json::object{};
    }

    if (auto msg = rec[expr::smessage]) {
        entry["message"] = *msg;
    }

    strm << json::serialize(entry);
}

}  // namespace

void InitLogging() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true);
}

void LogInfo(std::string_view message, json::object data) {
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(std::move(data)))
                             << message;
}

void LogServerStarted(unsigned port, std::string_view address) {
    json::object data;
    data["port"] = port;
    data["address"] = std::string(address);
    LogInfo("server started", std::move(data));
}

void LogServerExited(int code, std::optional<std::string_view> exception_message) {
    json::object data;
    data["code"] = code;
    if (exception_message) {
        data["exception"] = std::string(*exception_message);
    }
    LogInfo("server exited", std::move(data));
}

void LogRequestReceived(std::string_view ip, std::string_view uri, std::string_view method) {
    json::object data;
    data["ip"] = std::string(ip);
    data["URI"] = std::string(uri);
    data["method"] = std::string(method);
    LogInfo("request received", std::move(data));
}

void LogResponseSent(std::string_view ip, long long response_time_ms, int code,
                     std::optional<std::string_view> content_type) {
    json::object data;
    data["ip"] = std::string(ip);
    data["response_time"] = response_time_ms;
    data["code"] = code;
    if (content_type) {
        data["content_type"] = std::string(*content_type);
    } else {
        data["content_type"] = nullptr;
    }
    LogInfo("response sent", std::move(data));
}

void LogError(int code, std::string_view text, std::string_view where) {
    json::object data;
    data["code"] = code;
    data["text"] = std::string(text);
    data["where"] = std::string(where);
    LogInfo("error", std::move(data));
}

}  // namespace server_logging
