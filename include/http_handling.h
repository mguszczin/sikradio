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
inline constexpr string_view DOUBLE_CRLF = "\r\n\r\n";

/* We only allow response in format of 3XX and 2XX */
enum class ParsedStatus { HTTP_OK, HTTP_MOVED };

struct ParsedHttpResponse {
    ParsedStatus status;
    optional<string> location;
    optional<string> cookie;
    optional<size_t> icy_metaint;
};

ParsedHttpResponse parse_http_response(string_view http_request);

string get_http_request_string(const Url &url, bool is_multiplex,
                               optional<string> cookie);

} // namespace http

#endif