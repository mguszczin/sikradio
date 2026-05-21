#include "buffer_handling.h"

#include <array>
#include <cerrno>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <poll.h>
#include <sys/socket.h>

#include "http_handling.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace {
using std::array;
using std::generic_category;
using std::nullopt;
using std::optional;
using std::runtime_error;
using std::string;
using std::string_view;
using std::system_error;
using std::to_string;
} // namespace

namespace http {

SslOperationResult evaluate_ssl_error(SSL *ssl, int return_code,
                                      bool is_reading, int fd)
{
    if (return_code > 0)
        return SslOperationResult::Success;

    int err = SSL_get_error(ssl, return_code);

    switch (err) {
    case SSL_ERROR_WANT_READ:
        return SslOperationResult::NeedsRead;

    case SSL_ERROR_WANT_WRITE:
        return SslOperationResult::NeedsWrite;

    case SSL_ERROR_ZERO_RETURN:
        return SslOperationResult::Closed;

    case SSL_ERROR_SYSCALL:
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return is_reading ? SslOperationResult::NeedsRead
                              : SslOperationResult::NeedsWrite;
        }

        if (errno != 0) {
            throw system_error(errno, generic_category(),
                               "SSL Syscall error on fd " + to_string(fd));
        } else if (return_code == 0) {
            throw runtime_error("SSL Syscall error on fd " + to_string(fd) +
                                ": unexpected EOF");
        }
        throw runtime_error("Unknown SSL Syscall error on fd " + to_string(fd));

    case SSL_ERROR_SSL: {
        unsigned long err_queue_code = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err_queue_code, err_buf, sizeof(err_buf));
        throw runtime_error(string("Fatal OpenSSL protocol error: ") + err_buf);
    }

    default:
        throw runtime_error("Unknown SSL error code: " + to_string(err));
    }
}

bool Writer::is_finished() const noexcept
{
    return bytes_sent >= buffer.size();
}

SocketStatus Writer::write_using_http(const struct pollfd &pfd)
{
    if (is_finished())
        return SocketStatus::Finished;

    const char *data_ptr = buffer.data() + bytes_sent;
    size_t data_len = buffer.size() - bytes_sent;

    ssize_t sent = send(pfd.fd, data_ptr, data_len, MSG_NOSIGNAL);

    if (sent > 0) {
        bytes_sent += sent;

        if (is_finished()) {
            return SocketStatus::Finished;
        }

        return SocketStatus::Continuing;
    }

    if (sent < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            return SocketStatus::Continuing;
        }

        throw system_error(errno, generic_category(),
                           "Failed to write to socket");
    }

    throw runtime_error("send() returned 0 unexpectedly");
}

SocketStatus Writer::write_using_https(struct pollfd &pfd, SSL *ssl)
{
    if (is_finished())
        return SocketStatus::Finished;

    const char *data_ptr = buffer.data() + bytes_sent;
    size_t data_len = buffer.size() - bytes_sent;

    int sent = SSL_write(ssl, data_ptr, static_cast<int>(data_len));

    SslOperationResult res = evaluate_ssl_error(ssl, sent, false, pfd.fd);

    switch (res) {
    case SslOperationResult::Success:
        bytes_sent += sent;

        if (is_finished()) {
            return SocketStatus::Finished;
        }
        return SocketStatus::Continuing;

    case SslOperationResult::NeedsRead:
        pfd.events = POLLIN;
        return SocketStatus::Continuing;

    case SslOperationResult::NeedsWrite:
        pfd.events = POLLOUT;
        return SocketStatus::Continuing;

    case SslOperationResult::Closed:
        throw runtime_error("Connection closed during ssl handshake");

    default:
        throw runtime_error("Unexpected SSL operation result during write");
    }
}

SocketStatus Writer::write_to_socket(struct pollfd &pfd, SSL *ssl)
{
    if (ssl)
        return write_using_https(pfd, ssl);
    else
        return write_using_http(pfd);
}

void Writer::restart() noexcept { bytes_sent = 0; }

void Writer::change_buffer(std::string to_write)
{
    buffer = std::move(to_write);
    bytes_sent = 0;
}

SocketStatus Reader::read_using_http(const struct pollfd &pfd)
{
    array<char, PAGE_SIZE> reading_buffer;

    ssize_t status =
        recv(pfd.fd, reading_buffer.data(), reading_buffer.size(), 0);

    if (status > 0) {
        buffer.append(reading_buffer.begin(), reading_buffer.begin() + status);
        return SocketStatus::Continuing;
    } else if (status == 0) {
        return SocketStatus::ConnectionClosed;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return SocketStatus::Continuing;
        }

        throw system_error(errno, generic_category(),
                           "Failed to read from socket");
    }
}

SocketStatus Reader::read_using_https(struct pollfd &pfd, SSL *ssl)
{
    array<char, PAGE_SIZE> reading_buffer;

    int status = SSL_read(ssl, reading_buffer.data(), reading_buffer.size());

    SslOperationResult res = evaluate_ssl_error(ssl, status, true, pfd.fd);

    switch (res) {
    case SslOperationResult::Success:
        buffer.append(reading_buffer.begin(), reading_buffer.begin() + status);
        return SocketStatus::Continuing;

    case SslOperationResult::NeedsRead:
        pfd.events = POLLIN;
        return SocketStatus::Continuing;

    case SslOperationResult::NeedsWrite:
        pfd.events = POLLOUT;
        return SocketStatus::Continuing;

    case SslOperationResult::Closed:
        throw runtime_error("Connection closed during ssl handshake");

    default:
        throw runtime_error("Unexpected SSL operation result during read");
    }
}

SocketStatus Reader::read_from_socket(struct pollfd &pfd, SSL *ssl)
{
    if (ssl)
        return read_using_https(pfd, ssl);
    else
        return read_using_http(pfd);
}

optional<string> Reader::try_to_fetch_header()
{
    if (!can_extract_header())
        return nullopt;

    size_t pos = buffer.find(DOUBLE_CRLF);

    if (pos == string::npos) {
        throw runtime_error(
            "HTTP headers exceeded maximum allowed size without termination");
    }

    size_t split_point = pos + DOUBLE_CRLF.length();

    string header = buffer.substr(0, split_point);

    buffer.erase(0, split_point);

    return header;
}

string Reader::restart() noexcept
{
    is_header_present = false;
    last_time_asked = 0;

    string tmp = std::move(buffer);
    buffer.clear();
    return tmp;
}

bool Reader::can_extract_header() noexcept
{
    size_t search_start = (last_time_asked >= 3) ? last_time_asked - 3 : 0;

    size_t pos = buffer.find(DOUBLE_CRLF, search_start);
    last_time_asked = buffer.size();

    if (pos != string_view::npos) {
        is_header_present = true;
    } else if (buffer.size() >= 2 * PAGE_SIZE) {
        is_header_present = true;
    }

    return is_header_present;
}

} // namespace http