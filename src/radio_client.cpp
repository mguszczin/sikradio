#include "radio_client.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <format>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <arpa/inet.h>
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

using logs::Verbosity;

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
using std::strftime;
using std::string;
using std::system_error;
using std::to_string;

using url::parse_url;
using url::Url;

string get_current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y.%m.%d %H.%M.%S", &tm_buf);
    return std::string(buf);
}

string get_peer_address(int sockfd)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getpeername(sockfd, (struct sockaddr *)&addr, &len) == -1) {
        return "unknown";
    }

    char ipstr[INET6_ADDRSTRLEN];
    int port;

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
        return format("{}:{}", ipstr, port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
        return format("[{}]:{}", ipstr, port);
    }
}

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
    logs::server_info("{}\nresolving name {}", get_current_timestamp(),
                      url.address);

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

    logs::server_info("connecting to server {}", get_peer_address(sockfd));

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

    cur_url = parse_url(response.location.value());

    string new_request =
        get_http_request_string(cur_url, is_multiplexing, response.cookie);

    writer.change_buffer(new_request);
    establish_connection(cur_url, poll_fd);

    logs::server_info("{}", new_request.substr(0, new_request.size() - 2));

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

    if (pot_headers) {
        logs::server_info("{}", pot_headers.value().substr(
                                    0, pot_headers.value().size() - 2));
        handle_new_headers(poll_fd, pot_headers.value());
    }

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

bool RadioClient::handle_user_input(pollfd &poll_fd)
{
    if (poll_fd.revents & POLLERR) {
        throw std::runtime_error("poll() reported POLLERR on standard input.");
    }

    if (poll_fd.revents & POLLHUP) {
        logs::debug("Standard input disconnected (POLLHUP). Continuing "
                    "background playback.");
        poll_fd.fd = -1;
        return false;
    }

    if (poll_fd.revents & POLLIN) {
        string line;

        if (getline(cin, line)) {
            if (line == QUIT_MESSAGE) {
                return true;
            }
        } else {
            if (cin.eof()) {
                logs::debug("Standard input reached EOF. Continuing playback.");
                poll_fd.fd = -1;
                return false;
            } else {
                throw std::runtime_error(
                    "std::getline failed with a stream error.");
            }
        }
    }
    return false;
}

void RadioClient::handle_timeout(struct pollfd &poll_fd)
{
    logs::server_info("data receiving timeout");

    reader.restart();
    writer.restart();

    establish_connection(cur_url, poll_fd);

    string request = get_http_request_string(cur_url, is_multiplexing, nullopt);
    writer.change_buffer(request);
    logs::server_info("{}", request.substr(0, request.size() - 2));
}

void RadioClient::start()
{
    static constexpr size_t INPUT_FD = 0, SERVER_FD = 1;
    array<struct pollfd, LISTENING_POINTS> poll_fds{};

    string request = get_http_request_string(cur_url, is_multiplexing, nullopt);
    writer.change_buffer(request);

    poll_fds[INPUT_FD].fd = STDIN_FILENO;
    poll_fds[INPUT_FD].events = POLLIN;

    establish_connection(cur_url, poll_fds[SERVER_FD]);
    logs::server_info("{}", request.substr(0, request.size() - 2));

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
            handle_timeout(poll_fds[SERVER_FD]);
            continue;
        }

        if (handle_user_input(poll_fds[INPUT_FD]) ||
            handle_server_comunication(poll_fds[SERVER_FD]))
            break;
    }
}

} // namespace client