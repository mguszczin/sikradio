#include "radio_client.h"

#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "url.h"

namespace {

using program_arguments::IpType;
using program_arguments::Verbosity;

using std::format;
using std::optional;
using std::runtime_error;
using std::string;
using std::to_string;

using url::Url;

string get_http_request_string(const Url &url, bool is_multiplex,
                               std::optional<std::string> cookie)
{
    string cookie_header =
        cookie.has_value() ? std::format("Cookie: {}\r\n", cookie.value()) : "";

    string icy_header = is_multiplex ? "Icy-MetaData: 1\r\n" : "";

    return format("GET {} HTTP/1.1\r\n"
                  "Host: {}\r\n"
                  "Connection: Keep-Alive\r\n"
                  "{}"
                  "{}"
                  "\r\n",
                  url.path, url.address, cookie_header, icy_header);
}

} // namespace

namespace client {
void RadioClient::set_up_addrinfo(struct addrinfo &hints) const
{
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;

    switch (radio_args.ip) {
    case IpType::IPv4:
        hints.ai_family = AF_INET;
        break;
    case IpType::IPv6:
        hints.ai_family = AF_INET6;
        break;
    case IpType::Default:
    default:
        hints.ai_family = AF_UNSPEC;
        break;
    }
}

int RadioClient::establish_connection(const Url &url) const
{
    struct addrinfo hints, *res;

    set_up_addrinfo(hints);
    string port_str = to_string(url.port);

    int status =
        getaddrinfo(url.address.c_str(), port_str.c_str(), &hints, &res);

    if (status != 0) {
        throw runtime_error(format("Failed to resolve host '{}:{}': {}",
                                   url.address, url.port,
                                   gai_strerror(status)));
    }

    int sockfd{-1};
    for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        throw runtime_error(
            format("Could not connect to '{}:{}'", url.address, url.port));
    }

    return sockfd;
}

int RadioClient::find_radio_server(const Url &url) const
{
    Url cur_url = url;
    optional<string> cookie;
    bool found = false;
    do {
        int cur_fd = establish_connection(cur_url);
        string request = get_http_request_string(
            cur_url, radio_args.is_multiplexing, cookie);

        if (cur_url.is_https)
            auto [cur_url, found] = handle_https(std::move(request));
        else
            auto [cur_url, found] = handle_http(std::move(request));

    } while (!found);
}

void RadioClient::start()
{
    std
    // some kind of polling
}

} // namespace client