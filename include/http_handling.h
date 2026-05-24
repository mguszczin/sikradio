#ifndef HTTP_HANDLING_H
#define HTTP_HANDLING_H

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "url.h"

namespace http {

inline constexpr std::string_view CRLF = "\r\n";
inline constexpr std::string_view DOUBLE_CRLF = "\r\n\r\n";

/* We only allow response in format of 3XX and 2XX */
enum class ParsedStatus { HTTP_OK, HTTP_MOVED };

/**
 * Structure representing Parsed http response. Parsed http response is allowed
 * to have:
 * - Status (see enum).
 * - A few cookies in format 'a=b'.
 * - Icy metaint which specifies metaint for multiplexing
 *
 * The given constants specify in what form the `header_tag` should be inside
 * the http response.
 */
struct ParsedHttpResponse {
    ParsedStatus status;
    static constexpr std::string_view HEADER_LOCATION = "location";
    static constexpr std::string_view HEADER_SET_COOKIE = "set-cookie";
    static constexpr std::string_view HEADER_ICY_METAINT = "icy-metaint";

    std::optional<std::string> location;
    std::map<std::string, std::string> cookies;
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
std::string
get_http_request_string(const url::Url &url, bool is_multiplex,
                        const std::map<std::string, std::string> &cookies);

} // namespace http

#endif