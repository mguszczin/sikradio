#include "http_handling.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "url.h"

namespace {

using std::errc;
using std::format;
using std::from_chars;
using std::invalid_argument;
using std::nullopt;
using std::stoull;
using std::string;
using std::string_view;
using std::transform;

using http::CRLF;
using http::DOUBLE_CRLF;
using http::ParsedHttpResponse;
using http::ParsedStatus;

ParsedStatus check_response_line_parts(string_view protocol,
                                       string_view status_code,
                                       string_view reason_phrase)
{
    if (protocol.empty() || status_code.empty() || reason_phrase.empty()) {
        throw invalid_argument("Response line parts cannot be empty.");
    }

    if (protocol != "HTTP/1.0" && protocol != "HTTP/1.1" && protocol != "ICY") {
        throw invalid_argument(
            format("Unsupported or invalid protocol: '{}'", protocol));
    }

    if (status_code.size() != 3) {
        throw invalid_argument(
            format("Status code must be exactly 3 digits: '{}'", status_code));
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

    throw invalid_argument(
        format("Unrecognized or unhandled status code: '{}'", status_code));
}

ParsedStatus parse_response_line(string_view response_line)
{
    if (response_line.size() < 2 ||
        response_line.substr(response_line.size() - 2) != CRLF) {
        throw invalid_argument(format(
            "Response line missing CRLF terminator:\n{}", response_line));
    }

    string_view line = response_line.substr(0, response_line.size() - 2);

    size_t first_space = line.find(' ');
    if (first_space == string_view::npos) {
        throw invalid_argument(
            format("Missing space after protocol in response line:\n{}",
                   response_line));
    }

    size_t second_space = line.find(' ', first_space + 1);
    if (second_space == string_view::npos) {
        throw invalid_argument(
            format("Missing space after status code in response line:\n{}",
                   response_line));
    }

    if (second_space == first_space + 1) {
        throw invalid_argument(
            "Empty status code section (double space detected).");
    }

    string_view protocol = line.substr(0, first_space);
    string_view status_code =
        line.substr(first_space + 1, second_space - first_space - 1);
    string_view reason_phrase = line.substr(second_space + 1);

    return check_response_line_parts(protocol, status_code, reason_phrase);
}

string to_lower_string(string_view sv)
{
    string result(sv);
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return tolower(c); });
    return result;
}

string_view strip(string_view sv)
{
    constexpr string_view whitespace = " \t\r";

    size_t start = sv.find_first_not_of(whitespace);
    if (start == string_view::npos) {
        return {};
    }

    size_t end = sv.find_last_not_of(whitespace);
    return sv.substr(start, end - start + 1);
}

void process_header(ParsedHttpResponse &result, string_view key,
                    string_view value, bool is_content_mpeg_present)
{
    static constexpr string_view HEADER_LOCATION = "location";
    static constexpr string_view HEADER_SET_COOKIE = "set-cookie";
    static constexpr string_view HEADER_ICY_METAINT = "icy-metaint";

    static constexpr string_view HEADER_CONTENT_TYPE = "content-type";
    static constexpr string_view HEADER_CONTENT_VALUE = "audio/mpeg";

    key = strip(key);
    value = strip(value);

    if (key.empty()) {
        return;
    }

    string key_lower = to_lower_string(key);

    if (key_lower == HEADER_LOCATION) {
        result.location = string(value);
    } else if (key_lower == HEADER_SET_COOKIE) {
        result.cookie = string(value);
    } else if (key_lower == HEADER_ICY_METAINT) {
        size_t metaint_val;

        auto [ptr, ec] =
            from_chars(value.data(), value.data() + value.size(), metaint_val);

        if (ec == errc()) {
            result.icy_metaint = metaint_val;
        }
    } else if (key_lower == HEADER_CONTENT_TYPE) {
        string value_lower = to_lower_string(value);
        if (value == HEADER_CONTENT_VALUE)
            is_content_mpeg_present = true;
    }
}

[[nodiscard]] ParsedHttpResponse parse_headers(string_view headers,
                                               ParsedStatus status)
{
    ParsedHttpResponse result;
    result.status = status;

    size_t current_pos = 0;
    bool content_type_present = false;

    while (current_pos < headers.size()) {
        size_t next_crlf = headers.find(CRLF, current_pos);

        if (next_crlf == string_view::npos || next_crlf == current_pos) {
            break;
        }

        string_view header_line =
            headers.substr(current_pos, next_crlf - current_pos);
        size_t colon_pos = header_line.find(':');

        if (colon_pos != string_view::npos) {
            string_view key = header_line.substr(0, colon_pos);
            string_view value = header_line.substr(colon_pos + 1);

            process_header(result, key, value, content_type_present);
        }

        current_pos = next_crlf + CRLF.size();
    }

    if (!content_type_present && status == ParsedStatus::HTTP_OK)
        throw invalid_argument(
            "Content type wasn't present inside the HTTP OK response");

    return result;
}

} // namespace

namespace http {

ParsedHttpResponse parse_http_response(string_view http_response)
{

    size_t end_of_headers = http_response.find(DOUBLE_CRLF);
    if (end_of_headers == string_view::npos)
        throw invalid_argument(format("No {} inside the http response:\n{}",
                                      DOUBLE_CRLF, http_response));

    size_t first_crlf = http_response.find(CRLF);
    string_view first_line = http_response.substr(0, first_crlf + 2);
    ParsedStatus status = parse_response_line(first_line);

    size_t headers_start = first_crlf + 2;
    size_t headers_len = end_of_headers + DOUBLE_CRLF.size() - headers_start;

    string_view headers = http_response.substr(headers_start, headers_len);

    return parse_headers(headers, status);
}

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