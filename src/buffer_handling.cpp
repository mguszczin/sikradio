#include "buffer_handling.h"

#include <cerrno>
#include <poll.h>
#include <sys/socket.h>

namespace {

}

namespace http {

bool Writer::is_finished() const noexcept
{
    return bytes_sent >= buffer.size();
}

WriterStatus Writer::write_to_socket(const struct pollfd &pfd)
{
    if (!(pfd.revents & POLLOUT)) {
        return WriterStatus::StillWriting;
    }

    const char *data_ptr = buffer.data() + bytes_sent;
    size_t data_len = buffer.size() - bytes_sent;

    ssize_t sent = send(pfd.fd, data_ptr, data_len, MSG_NOSIGNAL);

    if (sent > 0) {
        bytes_sent += sent;

        if (is_finished()) {
            return WriterStatus::Finished;
        }

        return WriterStatus::StillWriting;
    }

    if (sent < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return WriterStatus::StillWriting;
        }

        if (errno == EPIPE || errno == ECONNRESET) {
            return WriterStatus::ConnectionClosed;
        }

        return WriterStatus::Error;
    }

    return WriterStatus::Error;
}

void Writer::restart() noexcept { bytes_sent = 0; }

void Writer::change_buffer(std::string to_write)
{
    buffer = std::move(to_write);
}

} // namespace http