#include "http_handling.h"

#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "url.h"

namespace {

using std::format;
using std::invalid_argument;
using std::nullopt;
using std::string_view;

using http::CRLF;
using http::ParsedStatus;

ParsedStatus check_response_line_parts(string_view protocol,
                                       string_view status_code,
                                       string_view reason_phrase) noexcept
{
    if (protocol.empty() || status_code.empty() || reason_phrase.empty()) {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    if (protocol != "HTTP/1.0" && protocol != "HTTP/1.1" && protocol != "ICY") {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    if (status_code.size() != 3) {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    char first_digit = status_code[0];

    if (first_digit == '2') {
        if (status_code == "200") {
            return ParsedStatus::HTTP_OK;
        }
    } else if (first_digit == '3') {
        if (status_code == "301" || status_code == "302") {
            return ParsedStatus::HTTP_MOVED;
        }
    }

    return ParsedStatus::FAILED_TO_PARSE;
}

ParsedStatus parse_response_line(string_view response_line) noexcept
{
    if (response_line.size() < 2 ||
        response_line.substr(response_line.size() - 2) != CRLF)
        return ParsedStatus::FAILED_TO_PARSE;
    string_view line = response_line.substr(0, response_line.size() - 2);

    size_t first_space = line.find(' ');
    if (first_space == string_view::npos) {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    size_t second_space = line.find(' ', first_space + 1);
    if (second_space == string_view::npos || second_space == first_space + 1) {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    if (line.find(' ', second_space + 1) != string_view::npos) {
        return ParsedStatus::FAILED_TO_PARSE;
    }

    string_view protocol = line.substr(0, first_space);
    string_view status_code =
        line.substr(first_space + 1, second_space - first_space - 1);
    string_view reason_phrase = line.substr(second_space + 1);

    return check_response_line_parts(protocol, status_code, reason_phrase);
}

} // namespace

namespace http {

[[nodiscard]] ParsedHttpResponse parse_http_response(string_view http_response)
{
    static const ParsedHttpResponse WRONG_PARSE = {
        ParsedStatus::FAILED_TO_PARSE, nullopt, nullopt, nullopt};
    size_t first_crlf = http_response.find(CRLF);

    if (first_crlf == string_view::npos) {
        return WRONG_PARSE;
    }

    string_view first_line = http_response.substr(0, first_crlf + 2);
    ParsedStatus status = parse_response_line(first_line);

    if (status == ParsedStatus::FAILED_TO_PARSE) {
        return WRONG_PARSE;
    }

    string_view headers_text = http_response.substr(first_crlf + 2);

    return parse_headers(headers_text, status);
}

[[nodiscard]] string get_http_request_string(const Url &url, bool is_multiplex,
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