#include "radio_client.h"

#include <cstring>
#include <format>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

using program_arguments::IpType;
using program_arguments::Verbosity;

using std::format;
using std::runtime_error;
using std::string;
using std::to_string;

} // namespace

namespace client {

int RadioClient::establish_connection(const Url &url) const
{
    struct addrinfo hints, *res;
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
    do {
        int cur_fd = establish_connection(url);
    } while (true);
}

void RadioClient::start()
{
    int server_fd = find_radio_server(radio_args.url_address);

    // some kind of polling
}

} // namespace client