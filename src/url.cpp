#include "url.h"

#include <charconv>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include "verbosity.h"

namespace {

using std::errc;
using std::format;
using std::from_chars;
using std::invalid_argument;
using std::pair;
using std::string;
using std::string_view;

pair<size_t, bool> check_for_http_prefix(string_view url)
{
    static constexpr string_view HTTP_PREF = "http://";
    static constexpr string_view HTTPS_PREF = "https://";

    if (url.starts_with(HTTP_PREF)) {
        return {HTTP_PREF.size(), false};
    } else if (url.starts_with(HTTPS_PREF)) {
        return {HTTPS_PREF.size(), true};
    } else {
        throw invalid_argument(
            format("Invalid prefix of url: '{}'. Expected '{}' or '{}'.", url,
                   HTTP_PREF, HTTPS_PREF));
    }
}

size_t get_address_size(string_view url)
{
    size_t slash_pos = url.find('/');

    if (slash_pos == string_view::npos) {
        return url.size();
    }

    if (slash_pos == 0) {
        throw invalid_argument("No address provided in the url.");
    }

    return slash_pos;
}

pair<string, uint16_t> get_address_and_port(string_view host_port,
                                            bool is_https)
{
    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t HTTPS_PORT = 443;

    if (host_port.empty()) {
        throw invalid_argument(
            "Cannot parse address and port from an empty string.");
    }

    string_view host_part;
    string_view port_part;

    if (host_port.starts_with('[')) {
        const size_t close_bracket_pos = host_port.find(']');
        if (close_bracket_pos == string_view::npos) {
            throw invalid_argument(format(
                "Malformed IPv6 address: missing closing bracket in '{}'",
                host_port));
        }

        host_part = host_port.substr(1, close_bracket_pos - 1);

        const string_view remainder = host_port.substr(close_bracket_pos + 1);
        if (!remainder.empty()) {
            if (!remainder.starts_with(':')) {
                throw invalid_argument(
                    format("Malformed URL: expected ':' after ']', found '{}'",
                           remainder.front()));
            }
            port_part = remainder.substr(1);
        }
    } else {
        const size_t first_colon = host_port.find(':');
        const size_t last_colon = host_port.rfind(':');

        if (first_colon != string_view::npos) {
            if (first_colon == last_colon) {
                host_part = host_port.substr(0, first_colon);
                port_part = host_port.substr(first_colon + 1);
            } else {
                throw invalid_argument(
                    format("Malformed URL: Unbracketed IPv6 address or "
                           "multiple colons detected in '{}'",
                           host_port));
            }
        } else {
            host_part = host_port;
        }
    }

    uint16_t resolved_port = is_https ? HTTPS_PORT : HTTP_PORT;

    if (!port_part.empty()) {
        const auto *port_start = port_part.data();
        const auto *port_end = port_start + port_part.size();

        auto [ptr, ec] = from_chars(port_start, port_end, resolved_port);

        if (ec != errc{} || ptr != port_end) {
            throw invalid_argument(
                format("Invalid port number: '{}'", port_part));
        }

        logs::debug("Custom port detected: {}", resolved_port);
    }

    return {string(host_part), resolved_port};
}

} // namespace

namespace url {

[[nodiscard]] Url parse_url(string_view url)
{
    logs::debug("Parsing url: {}", url);
    auto [prefix_size, is_https] = check_for_http_prefix(url);

    string_view url_without_http_pref = url.substr(prefix_size);

    size_t slash_pos = get_address_size(url_without_http_pref);

    string_view host_port = url_without_http_pref.substr(0, slash_pos);

    auto [address, port] = get_address_and_port(host_port, is_https);

    string_view path = url_without_http_pref.substr(slash_pos);

    if (path.empty()) {
        path = "/";
    }

    logs::debug(
        "Parsed result -> Address: '{}', Port: {}, Path: '{}', HTTPS: {}",
        address, port, path, is_https);

    return Url{.address = address,
               .port = port,
               .path = string{path},
               .is_https = is_https};
}

} // namespace url