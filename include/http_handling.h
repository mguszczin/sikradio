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

enum class ParsedStatus { HTTP_OK, HTTP_MOVED, FAILED_TO_PARSE };

struct ParsedHttp {
    ParsedStatus status;
    optional<Url> url;
    optional<string> cookie;
};

ParsedHttp parse_http_request(string_view http_request);

string get_http_request_string(const Url &url, bool is_multiplex,
                               optional<string> cookie);

} // namespace http

#endif