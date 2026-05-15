#include "buffer_handling.h"

#include <array>
#include <cerrno>
#include <optional>
#include <string_view>
#include <system_error>

#include <poll.h>
#include <sys/socket.h>

#include "http_handling.h"

namespace {
using std::array;
using std::generic_category;
using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using std::system_error;
} // namespace

namespace http {

bool Writer::is_finished() const noexcept
{
    return bytes_sent >= buffer.size();
}

SocketStatus Writer::write_to_socket(const struct pollfd &pfd)
{
    if (pfd.revents & (POLLERR | POLLHUP)) {
        return SocketStatus::ConnectionClosed;
    }

    if (!(pfd.revents & POLLOUT)) {
        return SocketStatus::Continuing;
    }

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
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SocketStatus::Continuing;
        }

        if (errno == EPIPE || errno == ECONNRESET) {
            return SocketStatus::ConnectionClosed;
        }

        return SocketStatus::Error;
    }

    return SocketStatus::Error;
}

void Writer::restart() noexcept { bytes_sent = 0; }

void Writer::change_buffer(std::string to_write)
{
    buffer = std::move(to_write);
}

SocketStatus Reader::read_from_socket(const pollfd &pfd)
{
    if (pfd.revents & (POLLERR | POLLHUP)) {
        return SocketStatus::ConnectionClosed;
    }

    if (!(pfd.revents & POLLIN)) {
        return SocketStatus::Continuing;
    }

    array<char, PAGE_SIZE> reading_buffer;

    ssize_t status =
        recv(pfd.fd, reading_buffer.data(), reading_buffer.size(), 0);

    if (status > 0) {
        buffer.append(reading_buffer.begin(), reading_buffer.begin() + status);
        return SocketStatus::Continuing;
    } else if (status == 0) {
        return SocketStatus::ConnectionClosed;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SocketStatus::Continuing;
        }

        throw system_error(errno, generic_category(),
                           "Failed to read from socket");
    }
}

optional<string> Reader::try_to_fetch_header()
{
    if (!can_extract_header())
        return nullopt;

    static constexpr std::string_view delimiter = "\r\n\r\n";

    size_t pos = buffer.find(DOUBLE_CRLF);
    size_t split_point = pos + delimiter.length();

    string header = buffer.substr(0, split_point);

    buffer.erase(0, split_point);

    return header;
}

string Reader::restart() noexcept
{
    is_header_present = false;
    last_time_asked = 0;

    string tmp = std::move(buffer);
    buffer = "";
    return tmp;
}

bool Reader::can_extract_header() noexcept
{
    size_t pos = buffer.find(DOUBLE_CRLF, last_time_asked);
    last_time_asked = buffer.size();
    if (pos != string_view::npos || buffer.size() >= 2 * PAGE_SIZE)
        is_header_present = true;

    return is_header_present;
}

} // namespace http