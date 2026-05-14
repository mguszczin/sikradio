#include "http_handling.h"

#include <format>
#include <optional>
#include <string>

#include "url.h"

namespace {
using std::format;
using std::optional;
using std::string;

using url::Url;
} // namespace

namespace http {

string get_http_request_string(const Url &url, bool is_multiplex,
                               optional<string> cookie)
{
    string cookie_header =
        cookie.has_value() ? format("Cookie: {}\r\n", cookie.value()) : "";

    string icy_header = is_multiplex ? "Icy-MetaData: 1\r\n" : "";

    return format("GET {} HTTP/1.1\r\n"
                  "Host: {}\r\n"
                  "Connection: Keep-Alive\r\n"
                  "{}"
                  "{}"
                  "\r\n",
                  url.path, url.address, cookie_header, icy_header);
}

} // namespace http