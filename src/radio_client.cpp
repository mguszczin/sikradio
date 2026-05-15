#include "radio_client.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <deque>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <fcntl.h>
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

int RadioClient::find_matching_address(
    const struct addrinfo *res) const noexcept
{
    int sockfd{-1};
    for (const struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(sockfd);
        sockfd = -1;
    }
    return sockfd;
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

    int sockfd = find_matching_address(res);
    freeaddrinfo(res);

    if (sockfd == -1) {
        throw runtime_error(
            format("Could not connect to '{}:{}'", url.address, url.port));
    }

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
        throw runtime_error("Could not set socket to nonblocking mode");
    }

    return sockfd;
}

void RadioClient::start()
{
    static constexpr size_t INPUT_FD = 0, SERVER_FD = 1;
    Url cur_url = radio_args.url_address;
    array<struct pollfd, LISTENING_POINTS> poll_fds{};

    ClientModes cur_mode = ClientModes::SendingData;

    poll_fds[INPUT_FD].fd = STDIN_FILENO;
    poll_fds[INPUT_FD].events = POLLIN;

    poll_fds[SERVER_FD].fd = establish_connection(cur_url);
    poll_fds[SERVER_FD].events = POLLOUT;

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
        if (ClientModes::ReadingData) {
            should_break, new_url = handle_read(poll_fds);
        } else
            should_break, can_change = handle_write(poll_fds);
    }
}

} // namespace client