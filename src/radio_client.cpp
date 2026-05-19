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

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "buffer_handling.h"
#include "http_handling.h"
#include "url.h"
#include "verbosity.h"

namespace {

using log::Verbosity;

using program_arguments::IpType;

using http::get_http_request_string;
using http::parse_http_response;
using http::ParsedHttpResponse;
using http::ParsedStatus;
using http::Socket;
using http::SocketStatus;

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

void RadioClient::set_up_addrinfo(struct addrinfo &hints) const noexcept
{
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;

    switch (ip) {
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

int RadioClient::connect_to_socket(const struct addrinfo *res) const noexcept
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

void RadioClient::establish_connection(const Url &url, struct pollfd &poll_fd)
{
    if (ssl != nullptr) {
        SSL_free(ssl);
        ssl = nullptr;
    }

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

    int sockfd = connect_to_socket(res);
    freeaddrinfo(res);

    if (sockfd == -1) {
        throw runtime_error(
            format("Could not connect to '{}:{}'", url.address, url.port));
    }

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
        throw system_error(errno, generic_category(),
                           "Failed to set socket to non-blocking");
    }

    server_socket = Socket{sockfd};

    poll_fd.fd = sockfd;

    if (url.is_https) {
        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            throw runtime_error("Failed to create SSL object structure");
        }
        SSL_set_fd(ssl, server_socket);
        SSL_set_tlsext_host_name(ssl, url.address.c_str());

        poll_fd.events = POLLIN | POLLOUT;
        mode = ClientModes::TlsHandshake;
        return;
    }

    poll_fd.events = POLLOUT;
    mode = ClientModes::SendingData;
}

void RadioClient::handle_new_headers(struct pollfd &poll_fd,
                                     const string &headers)
{
    ParsedHttpResponse response = parse_http_response(headers);

    if (response.status == ParsedStatus::HTTP_OK) {
        if (is_multiplexing) {
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
            printer.set_metaint(response.icy_metaint.value());
        }
        printer.print(reader.restart());
        mode = ClientModes::ReadingBody;
        return;
    }

    if (!response.location.has_value()) {
        throw invalid_argument(format(
            "No Location header given for HTTP 3xx redirect:\n{}", headers));
    }

    Url new_url = parse_url(response.location.value());

    string new_request =
        get_http_request_string(new_url, is_multiplexing, response.cookie);

    writer.change_buffer(new_request);
    establish_connection(new_url,
                         poll_fd); // not sure if we shouldn't do timout here?
                                   // don't know about accept
    return;
}

bool RadioClient::handle_sending_request(pollfd &poll_fd)
{
    auto response = writer.write_to_socket(poll_fd, ssl);
    if (response == SocketStatus::Finished) {
        mode = ClientModes::ReadingHeaders;
        poll_fd.events = POLLIN;
    }

    return (response == SocketStatus::ConnectionClosed);
}

bool RadioClient::handle_reading_headers(pollfd &poll_fd)
{
    auto response = reader.read_from_socket(poll_fd, ssl);
    optional<string> pot_headers = reader.try_to_fetch_header();

    if (pot_headers)
        handle_new_headers(poll_fd, pot_headers.value());

    return (response == SocketStatus::ConnectionClosed);
}

bool RadioClient::handle_reading_body(pollfd &poll_fd)
{
    auto response = reader.read_from_socket(poll_fd, ssl);
    printer.print(reader.restart());

    return (response == SocketStatus::ConnectionClosed);
}

bool RadioClient::handle_tls_handshake(struct pollfd &poll_fd)
{
    int ret = SSL_connect(ssl);
    if (ret == 1) {
        mode = ClientModes::SendingData;
        poll_fd.events = POLLOUT;
        return false;
    }

    int err = SSL_get_error(ssl, ret);

    switch (err) {
    case SSL_ERROR_WANT_READ:
        poll_fd.events = POLLIN;
        return false;
    case SSL_ERROR_WANT_WRITE:
        poll_fd.events = POLLOUT;
        return false;
    default:
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        throw runtime_error(format("TLS handshake failed: {}", err_buf));
    }
}

bool RadioClient::handle_server_comunication(pollfd &poll_fd)
{
    if (poll_fd.revents & (POLLHUP))
        return true;

    if (poll_fd.revents & (POLLERR)) {
        int error = 0;
        socklen_t errlen = sizeof(error);

        if (getsockopt(poll_fd.fd, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0)
            throw runtime_error(format("POLLERR on fd {}: {} (error code : {})",
                                       poll_fd.fd, strerror(error), error));
        else
            throw runtime_error(format(
                "POLLERR on fd {}: Failed to extract error via getsockopt",
                poll_fd.fd));
    }
    switch (mode) {
    case ClientModes::SendingData:
        return handle_sending_request(poll_fd);

    case ClientModes::ReadingHeaders:
        return handle_reading_headers(poll_fd);

    case ClientModes::ReadingBody:
        return handle_reading_body(poll_fd);

    case ClientModes::TlsHandshake:
        return handle_tls_handshake(poll_fd);

    default:
        throw invalid_argument(
            "Wrong enum inside `handle_server_communication`");
    }
}

bool RadioClient::handle_user_input(const pollfd &poll_fd)
{
    if (POLLIN & poll_fd.revents) {
        string line;
        if (getline(cin, line)) {
            if (line == QUIT_MESSAGE) {
                return true;
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
    writer.restart();
}

void RadioClient::start()
{
    static constexpr size_t INPUT_FD = 0, SERVER_FD = 1;
    array<struct pollfd, LISTENING_POINTS> poll_fds{};

    writer.change_buffer(
        get_http_request_string(cur_url, is_multiplexing, nullopt));

    poll_fds[INPUT_FD].fd = STDIN_FILENO;
    poll_fds[INPUT_FD].events = POLLIN;

    establish_connection(cur_url, poll_fds[SERVER_FD]);

    while (1) {
        int status = poll(poll_fds.data(), poll_fds.size(), timeout);

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