#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string_view>

namespace server_logging {

// Must be called once, before any other logging happens (e.g. first thing in main()).
void InitLogging();

// Low level helper: logs `message` at info severity together with an
// arbitrary JSON object as the "data" field. Prefer the typed helpers below
// for the events required by the assignment; use this directly only for
// ad-hoc logging.
void LogInfo(std::string_view message, boost::json::object data = {});

// server started
void LogServerStarted(unsigned port, std::string_view address);

// server exited
void LogServerExited(int code, std::optional<std::string_view> exception_message = std::nullopt);

// request received
void LogRequestReceived(std::string_view ip, std::string_view uri, std::string_view method);

// response sent
void LogResponseSent(std::string_view ip, long long response_time_ms, int code,
                     std::optional<std::string_view> content_type);

// error
void LogError(int code, std::string_view text, std::string_view where);

}  // namespace server_logging
