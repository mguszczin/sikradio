#include "http_handling.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

#include "socket.h"
#include "url.h"
#include "verbosity.h"

namespace {

using std::errc;
using std::format;
using std::from_chars;
using std::invalid_argument;
using std::isdigit;
using std::map;
using std::nullopt;
using std::optional;
using std::stoull;
using std::string;
using std::string_view;
using std::transform;

using http::CRLF;
using http::DOUBLE_CRLF;
using http::ParsedHttpResponse;
using http::ParsedStatus;

using url::Url;

#include <cctype>

// ...

ParsedStatus check_response_line_parts(string_view protocol,
                                       string_view status_code,
                                       string_view reason_phrase)
{
    if (protocol.empty() || status_code.empty() || reason_phrase.empty()) {
        throw invalid_argument("Protocol and status code cannot be empty.");
    }

    if (protocol != "HTTP/1.0" && protocol != "HTTP/1.1" && protocol != "ICY") {
        throw invalid_argument(
            format("Unsupported or invalid protocol: '{}'", protocol));
    }

    if (status_code.size() != 3 || !isdigit(status_code[0]) ||
        !isdigit(status_code[1]) || !isdigit(status_code[2])) {
        throw invalid_argument(format(
            "Status code must be exactly 3 numeric digits: '{}'", status_code));
    }

    char first_digit = status_code[0];

    if (first_digit == '2') {
        if (status_code == "200") {
            return ParsedStatus::HTTP_OK;
        }
    } else if (first_digit == '3') {
        return ParsedStatus::HTTP_MOVED;
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
    string result{sv};
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

void add_cookies(map<string, string> &cookies, string_view value)
{
    size_t semi_pos = value.find(';');
    if (semi_pos != string_view::npos) {
        value = value.substr(0, semi_pos);
    }

    size_t eq_pos = value.find('=');
    if (eq_pos != string_view::npos) {
        string_view cookie_key = strip(value.substr(0, eq_pos));
        string_view cookie_val = strip(value.substr(eq_pos + 1));

        if (!cookie_key.empty()) {
            cookies[string(cookie_key)] = string(cookie_val);
        }
    }
}

void process_header(ParsedHttpResponse &result, string_view key,
                    string_view value, bool &is_content_mpeg_present)
{

    static constexpr string_view HEADER_CONTENT_TYPE = "content-type";
    static constexpr string_view HEADER_CONTENT_VALUE = "audio/mpeg";

    key = strip(key);
    value = strip(value);

    if (key.empty()) {
        return;
    }

    string key_lower = to_lower_string(key);

    if (key_lower == ParsedHttpResponse::HEADER_LOCATION) {
        result.location = string{value};
    } else if (key_lower == ParsedHttpResponse::HEADER_SET_COOKIE) {
        add_cookies(result.cookies, value);
    } else if (key_lower == ParsedHttpResponse::HEADER_ICY_METAINT) {
        size_t metaint_val;

        auto [ptr, ec] =
            from_chars(value.data(), value.data() + value.size(), metaint_val);

        if (ec == errc() && ptr == value.data() + value.size()) {
            result.icy_metaint = metaint_val;
        }
    } else if (key_lower == HEADER_CONTENT_TYPE) {
        string value_lower = to_lower_string(value);
        if (value_lower == HEADER_CONTENT_VALUE)
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

    logs::debug(
        "Parsed Http Response: status: {}, location: {},"
        "icy_metaint: {}",
        static_cast<int>(result.status), result.location.value_or("none"),
        result.icy_metaint.has_value() ? std::to_string(*result.icy_metaint)
                                       : "none");
    return result;
}

} // namespace

namespace http {

ParsedHttpResponse parse_http_response(string_view http_response)
{
    logs::debug("http response to parse {}", http_response);

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
                               const map<string, string> &cookies)
{
    string cookie_header;

    if (!cookies.empty()) {
        cookie_header = "Cookie: ";
        bool first = true;
        for (const auto &[key, val] : cookies) {
            if (!first) {
                cookie_header += "; ";
            }
            cookie_header += format("{}={}", key, val);
            first = false;
        }
        cookie_header += "\r\n";
    }

    string icy_header = is_multiplex ? "Icy-MetaData: 1\r\n" : "";

    string host_val = url.address;
    if (host_val.find(':') != string::npos && host_val.front() != '[') {
        host_val = format("[{}]", host_val);
    }

    return format("GET {} HTTP/1.1\r\n"
                  "Host: {}\r\n"
                  "Connection: Keep-Alive\r\n"
                  "{}"
                  "{}"
                  "\r\n",
                  url.path, host_val, cookie_header, icy_header);
}

} // namespace http