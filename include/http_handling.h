#ifndef HTTP_HANDLING_H
#define HTTP_HANDLING_H

#include <optional>
#include <string>
#include <string_view>

#include "url.h"

namespace http {

inline constexpr std::string_view CRLF = "\r\n";
inline constexpr std::string_view DOUBLE_CRLF = "\r\n\r\n";

/* We only allow response in format of 3XX and 2XX */
enum class ParsedStatus { HTTP_OK, HTTP_MOVED };

struct ParsedHttpResponse {
    ParsedStatus status;
    std::optional<std::string> location;
    std::optional<std::string> cookie;
    std::optional<size_t> icy_metaint;
};

/**
 * @brief Parses a complete raw HTTP response text into a structured response
 * object.
 *
 * @throws `std::invalid_argument` If the headers are malformed, missing
 * required fields, or protocols are unsupported.
 */
ParsedHttpResponse parse_http_response(std::string_view http_request);

/**
 * @brief Generates a raw HTTP GET request formatted string.
 */
std::string get_http_request_string(const url::Url &url, bool is_multiplex,
                                    std::optional<std::string> cookie);

} // namespace http

#endif