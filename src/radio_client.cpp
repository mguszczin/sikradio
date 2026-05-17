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

#include "buffer_handling.h"
#include "http_handling.h"
#include "url.h"

namespace {

using program_arguments::IpType;
using program_arguments::Verbosity;

using http::get_http_request_string;
using http::parse_http_response;
using http::ParsedHttpResponse;
using http::ParsedStatus;

using std::array;
using std::cerr;
using std::cin;
using std::cout;
using std::deque;
using std::endl;
using std::format;
using std::generic_category;
using std::getline;
using std::invalid_argument;
using std::nullopt;
using std::optional;
using std::range_error;
using std::runtime_error;
using std::string;
using std::system_error;
using std::to_string;

using url::parse_url;
using url::Url;
} // namespace

namespace client {

void RadioClient::handle_new_headers(struct pollfd &poll_fd,
                                     const string &headers)
{
    ParsedHttpResponse response = parse_http_response(headers);

    if (response.status == ParsedStatus::HTTP_OK) {
        if (radio_args.is_multiplexing) {
            if (!response.icy_metaint.has_value()) {
                throw runtime_error(format("Multiplexing is ON, but no "
                                           "`icy-metaint` header found:\n{}",
                                           headers));
            }
            if (response.icy_metaint.value() == 0) {
                throw range_error(format("Invalid `icy-metaint` value of 0 "
                                         "received from server:\n{}",
                                         headers));
            }
        }

        cout << reader.restart();

        return;
    }

    if (!response.location.has_value()) {
        throw invalid_argument(format(
            "No Location header given for HTTP 3xx redirect:\n{}", headers));
    }

    Url new_url = parse_url(response.location.value());

    string new_request = get_http_request_string(
        new_url, radio_args.is_multiplexing, response.cookie);

    writer.change_buffer(new_request);
    establish_connection(new_url);
    poll_fd.fd = server_socket;
    return;
}

bool RadioClient::handle_user_input(const pollfd &poll_fd)
{
    if (POLLIN & poll_fd.revents) {
        string line;
        if (getline(cin, line)) {
            if (line == QUIT_MESSAGE) {
                return true;
            } else if (radio_args.verb == Verbosity::Debug) {
                cerr << line << endl;
            }
        } else {
            // this is not the expected behaviour remember that when
            // return you must close desc
            return true;
        }
    }
    return false;
}

void RadioClient::handle_timeout()
{
    if (mode == ClientModes::ReadingHeaders)
        reader.restart();

    mode = ClientModes::SendingData;
    reader.restart();
}

bool RadioClient::handle_server_comunication(pollfd &poll_fd)
{
    if (mode == ClientModes::SendingData) {
        auto response = writer.write_to_socket(poll_fd);
        if (response == http::SocketStatus::ConnectionClosed)
            return true;
        else if (response == http::SocketStatus::Continuing)
            return false;

        mode = ClientModes::ReadingHeaders;
        return false;
    } else {
        auto response = reader.read_from_socket(poll_fd);
        optional<string> pot_headers = reader.try_to_fetch_header();

        if (pot_headers) {
            handle_new_headers(poll_fd, pot_headers.value());
        }

        if (response == http::SocketStatus::ConnectionClosed)
            return true;
        else if (response == http::SocketStatus::Continuing)
            return false;

        mode = ClientModes::SendingData;
        return false;
    }
}

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

void RadioClient::establish_connection(const Url &url)
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
    server_socket = Socket{sockfd};
}

void RadioClient::start()
{
    static constexpr size_t INPUT_FD = 0, SERVER_FD = 1;
    array<struct pollfd, LISTENING_POINTS> poll_fds{};

    mode = ClientModes::SendingData;
    writer.change_buffer(
        get_http_request_string(cur_url, radio_args.is_multiplexing, nullopt));

    poll_fds[INPUT_FD].fd = STDIN_FILENO;
    poll_fds[INPUT_FD].events = POLLIN;

    poll_fds[SERVER_FD].fd = server_socket;
    poll_fds[SERVER_FD].events = POLLOUT;

    while (1) {
        int status = poll(poll_fds.data(), poll_fds.size(), radio_args.timeout);

        if (status < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw system_error(errno, generic_category(),
                               "Critical error in poll function");
        }

        if (status == 0) {
            handle_timeout();
            continue;
        }

        if (handle_user_input(poll_fds[INPUT_FD]) ||
            handle_server_comunication(poll_fds[SERVER_FD]))
            break;
    }
}

} // namespace client