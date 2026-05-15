#include "url.h"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using std::format;
using std::invalid_argument;
using std::pair;
using std::string;
using std::string_view;

pair<size_t, bool> check_for_http_prefix(string_view url)
{
    static constexpr string_view HTTP_PREF = "http://";
    static constexpr string_view HTTPS_PREF = "https://";

    if (url.starts_with(HTTP_PREF))
        return {HTTP_PREF.size(), false};
    else if (url.starts_with(HTTPS_PREF))
        return {HTTPS_PREF.size(), true};
    else {
        auto what =
            format("Invalid prefix of url: '{}'. Expected '{}' or '{}'.", url,
                   HTTP_PREF, HTTPS_PREF);
        throw invalid_argument(what);
    }
}

size_t get_address_size(string_view url)
{
    size_t slash_pos = url.find("/");

    if (slash_pos == string::npos) {
        return url.size();
    }

    if (slash_pos == 0) {
        throw invalid_argument("No address provided in the url.");
    }

    return slash_pos;
}

/** For now ignores ipv6 values inside */
pair<string, uint16_t> get_address_and_port(string_view host_port,
                                            bool is_https)
{
    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t HTTPS_PORT = 443;

    size_t colon_pos = host_port.find(':');

    if (colon_pos == string_view::npos) {
        return {string{host_port}, is_https ? HTTPS_PORT : HTTP_PORT};
    }

    string host = string(host_port.substr(0, colon_pos));
    string_view port_str = host_port.substr(colon_pos + 1);

    try {
        uint16_t port = static_cast<uint16_t>(stoul(string(port_str)));
        return {host, port};
    } catch (...) {
        throw invalid_argument(format("Invalid port number: '{}'", port_str));
    }
}

} // namespace

namespace url {

Url parse_url(string_view url)
{
    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t HTTPS_PORT = 443;

    auto [prefix_size, is_https] = check_for_http_prefix(url);

    string_view url_without_http_pref = url.substr(prefix_size);

    size_t slash_pos = get_address_size(url_without_http_pref);

    string_view host_port = url_without_http_pref.substr(0, slash_pos);

    auto [address, port] = get_address_and_port(host_port, is_https);

    string_view path = url_without_http_pref.substr(slash_pos);
    if (path.empty())
        path = "/";

    return Url{.address = address,
               .port = port,
               .path = string{path},
               .is_https = is_https};
}

} // namespace url