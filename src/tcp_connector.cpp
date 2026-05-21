#include "tcp_connector.h"

#include <cerrno>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "socket.h"

namespace {

using http::Socket;
using program_arguments::IpType;
using std::format;
using std::pair;
using std::runtime_error;
using std::string;
using std::to_string;
using url::Url;

} // namespace

namespace client {
AsyncConnector::AsyncConnector(IpType ip) : ip_type(ip), given_fd(-1) {}

AsyncConnector::~AsyncConnector() { cleanup(); }

void AsyncConnector::init(const Url &url)
{
    cleanup();
    struct addrinfo hints;
    set_up_addrinfo(hints);
    string port_str = to_string(url.port);

    int status = getaddrinfo(url.address.c_str(), port_str.c_str(), &hints,
                             &addr_list_head);
    if (status != 0) {
        throw runtime_error(format("Failed to resolve host '{}:{}': {}",
                                   url.address, url.port,
                                   gai_strerror(status)));
    }

    current_addr = addr_list_head;
}

pair<Socket, ConnectState> AsyncConnector::start_looking()
{
    if (given_fd != -1) {
        int error = 0;
        socklen_t errlen = sizeof(error);
        if (getsockopt(given_fd, SOL_SOCKET, SO_ERROR, &error, &errlen) < 0) {
            error = errno;
        }

        if (error == EINPROGRESS || error == EALREADY) {
            return {Socket(-1), ConnectState::Connecting};
        }

        if (error == 0) {
            cleanup();
            return {Socket(-1), ConnectState::Found};
        }

        given_fd = -1;
        if (current_addr != nullptr) {
            current_addr = current_addr->ai_next;
        }
    }

    while (current_addr != nullptr) {
        Socket sockfd{socket(current_addr->ai_family, current_addr->ai_socktype,
                             current_addr->ai_protocol)};
        if (!sockfd.is_valid()) {
            current_addr = current_addr->ai_next;
            continue;
        }

        if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
            current_addr = current_addr->ai_next;
            continue;
        }

        int status =
            connect(sockfd, current_addr->ai_addr, current_addr->ai_addrlen);

        if (status == 0) {
            cleanup();
            return {std::move(sockfd), ConnectState::Found};
        }

        if (errno == EINPROGRESS) {
            given_fd = sockfd;
            return {std::move(sockfd), ConnectState::Connecting};
        }

        current_addr = current_addr->ai_next;
    }

    cleanup();
    return {Socket(-1), ConnectState::NotFound};
}

void AsyncConnector::cleanup()
{
    given_fd = -1;
    if (addr_list_head != nullptr) {
        freeaddrinfo(addr_list_head);
        addr_list_head = nullptr;
        current_addr = nullptr;
    }
}

void AsyncConnector::set_up_addrinfo(struct addrinfo &hints) const noexcept
{
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;

    switch (ip_type) {
    case IpType::IPv4:
        hints.ai_family = AF_INET;
        break;
    case IpType::IPv6:
        hints.ai_family = AF_INET6;
        break;
    default:
        hints.ai_family = AF_UNSPEC;
        break;
    }
}

} // namespace client