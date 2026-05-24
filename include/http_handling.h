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

/**
 * Allowed response statuses.
 * We currently only support HTTP 200 (OK) and HTTP 3XX (Redirects/Moved).
 */
enum class ParsedStatus { HTTP_OK, HTTP_MOVED };

/**
 * Holds the extracted data from an HTTP response.
 *
 * - status: Whether the request succeeded (200) or redirected (3XX).
 * - location: The redirect URL (only present if status is HTTP_MOVED).
 * - cookies: Key-value pairs of any 'Set-Cookie' headers.
 * - icy_metaint: The number of audio bytes between metadata blocks (used for
 * Icecast).
 *
 * Note: The HEADER_* constants are written in lowercase because the parser
 * converts incoming server header keys to lowercase before checking them.
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
 * @brief Parses a raw HTTP response string into a structured object.
 *
 *
 * @throws std::invalid_argument if:
 * - The response is missing the double CRLF (\r\n\r\n) separator.
 * - The protocol is not HTTP/1.0, HTTP/1.1, or ICY.
 * - The status code is invalid, or anything other than 200 or 3XX.
 * - The status is 200 OK, but "Content-Type: audio/mpeg" is missing.
 */
ParsedHttpResponse parse_http_response(std::string_view http_response);

/**
 * @brief Generates a raw HTTP GET request string ready to be sent over a
 * socket.
 *
 * @return The fully formatted HTTP GET request string.
 */
std::string
get_http_request_string(const url::Url &url, bool is_multiplex,
                        const std::map<std::string, std::string> &cookies);

} // namespace http

#endif