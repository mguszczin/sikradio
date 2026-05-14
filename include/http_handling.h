#ifndef HTTP_HANDLING_H
#define HTTP_HANDLING_H

#include <optional>
#include <string>
#include <string_view>

#include "url.h"

namespace http {
using std::optional;
using std::string;
using std::string_view;

using url::Url;

inline constexpr string_view CRLF = "\r\n";

enum class ParsedStatus { HTTP_OK, HTTP_MOVED, FAILED_TO_PARSE };

struct ParsedHttpResponse {
    ParsedStatus status;
    optional<Url> url;
    optional<string> cookie;
    optional<size_t> icy_metaint;
};

[[nodiscard]] ParsedHttpResponse parse_http_response(string_view http_request);

[[nodiscard]] string get_http_request_string(const Url &url, bool is_multiplex,
                                             optional<string> cookie);

} // namespace http

#endif