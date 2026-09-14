#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string_view>

namespace server_logging {

void InitLogging();

void LogInfo(std::string_view message, boost::json::object data = {});

void LogServerStarted(unsigned port, std::string_view address);

void LogServerExited(int code, std::optional<std::string_view> exception_message = std::nullopt);

void LogRequestReceived(std::string_view ip, std::string_view uri, std::string_view method);

void LogResponseSent(std::string_view ip, long long response_time_ms, int code,
                     std::optional<std::string_view> content_type);

void LogError(int code, std::string_view text, std::string_view where);

}
