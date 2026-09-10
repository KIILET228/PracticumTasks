#include "request_handler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <system_error>
#include <unordered_map>

namespace http_handler {

using namespace std::literals;

namespace {

constexpr boost::beast::string_view kContentTypeText = "text/plain";

StringResponse MakeTextResponse(http::status status, std::string_view text, unsigned version, bool keep_alive,
                                bool include_body) {
    StringResponse response(status, version);
    response.set(http::field::content_type, kContentTypeText);
    response.keep_alive(keep_alive);
    if (include_body) {
        response.body() = std::string(text);
        response.content_length(response.body().size());
    } else {
        response.content_length(text.size());
    }
    return response;
}

std::string UrlDecode(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '%' && i + 2 < sv.size()) {
            const std::string hex{sv.substr(i + 1, 2)};
            char* end = nullptr;
            const long value = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + hex.size()) {
                result += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        if (sv[i] == '+') {
            result += ' ';
        } else {
            result += sv[i];
        }
    }
    return result;
}

boost::beast::string_view GetMimeType(const http_handler::fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    static const std::unordered_map<std::string, boost::beast::string_view> kExtensionToType = {
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };

    if (const auto it = kExtensionToType.find(ext); it != kExtensionToType.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

bool IsSubPath(http_handler::fs::path path, http_handler::fs::path base) {
    path = http_handler::fs::weakly_canonical(path);
    base = http_handler::fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

}

std::variant<StringResponse, FileResponse> RequestHandler::HandleFileRequest(http::verb method,
                                                                              std::string_view target,
                                                                              unsigned version,
                                                                              bool keep_alive) const {
    const bool include_body = (method != http::verb::head);

    if (method != http::verb::get && method != http::verb::head) {
        return MakeTextResponse(http::status::method_not_allowed, "Invalid method"sv, version, keep_alive,
                                include_body);
    }

    std::string decoded_target = UrlDecode(target);
    if (const auto query_pos = decoded_target.find('?'); query_pos != std::string::npos) {
        decoded_target.erase(query_pos);
    }
    if (!decoded_target.empty() && decoded_target.front() == '/') {
        decoded_target.erase(decoded_target.begin());
    }

    fs::path file_path = static_root_ / fs::path(decoded_target);

    if (!IsSubPath(file_path, static_root_)) {
        return MakeTextResponse(http::status::bad_request, "Bad request"sv, version, keep_alive, include_body);
    }

    std::error_code fs_ec;
    if (fs::is_directory(file_path, fs_ec)) {
        file_path /= "index.html";
    }

    http::file_body::value_type file;
    beast::error_code ec;
    file.open(file_path.string().c_str(), beast::file_mode::scan, ec);
    if (ec) {
        return MakeTextResponse(http::status::not_found, "File not found"sv, version, keep_alive, include_body);
    }

    const auto content_type = GetMimeType(file_path);

    if (!include_body) {
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, content_type);
        response.content_length(file.size());
        response.keep_alive(keep_alive);
        return response;
    }

    FileResponse response(std::piecewise_construct, std::make_tuple(std::move(file)),
                          std::make_tuple(http::status::ok, version));
    response.set(http::field::content_type, content_type);
    response.keep_alive(keep_alive);
    response.prepare_payload();
    return response;
}

}
