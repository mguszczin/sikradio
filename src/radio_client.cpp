#include "radio_client.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <deque>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "url.h"

namespace {

using program_arguments::IpType;
using program_arguments::Verbosity;

using std::array;
using std::cerr;
using std::cin;
using std::deque;
using std::endl;
using std::format;
using std::generic_category;
using std::getline;
using std::optional;
using std::runtime_error;
using std::string;
using std::system_error;
using std::to_string;

using url::Url;
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
    static constexpr size_t INPUT_FD = 0, SERVER_FD = 1;
    Url cur_url = radio_args.url_address;
    array<struct pollfd, LISTENING_POINTS> poll_fds{};

    ClientModes cur_mode = ClientModes::SendingData;

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = establish_connection(cur_url);
    poll_fds[1].events = POLLOUT;

    std::array<char, BUFFER_SIZE> audio_buffer;

    while (1) {
        int status = poll(poll_fds.data(), poll_fds.size(), radio_args.timeout);

        if (status < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw system_error(errno, generic_category(),
                               "Krytyczny błąd funkcji poll");
        }

        if (status == 0) {
            // handle_timeout();
            continue;
        }

        if (POLLIN & poll_fds[INPUT_FD].revents) {
            string line;
            if (getline(cin, line)) {
                if (line == QUIT_MESSAGE) {
                    break;
                } else if (radio_args.verb == Verbosity::Debug) {
                    cerr << line << endl;
                }
            } else {
                // this is not the expected behaviour remember that when
                // return you must close desc
                break;
            }
        }

        bool should_break{};
        if (ClientModes::ReadingData)
            should_break = handle_read(poll_fds);
        else
            should_break = handle_write(poll_fds);
    }
}

} // namespace client