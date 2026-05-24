#ifndef BUFFER_HANDLING_H
#define BUFFER_HANDLING_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <openssl/ssl.h>
#include <poll.h>

namespace http {

// standard page size
inline constexpr size_t PAGE_SIZE = 4096;

/**
 * Represents the state of a non-blocking socket operation.
 */
enum class SocketStatus {
    ConnectionClosed, // The peer cleanly closed the connection
    Continuing,       // Operation pending or partially complete; keep polling
    Finished          // The entire buffer was successfully sent
};

/**
 * Handles non-blocking writes to a socket (both plaintext HTTP and secure
 * HTTPS).
 */
class Writer {
  private:
    std::string buffer;
    size_t bytes_sent{0};

    bool is_finished() const noexcept;
    SocketStatus write_using_https(struct pollfd &poll_fd, SSL *ssl);
    SocketStatus write_using_http(const struct pollfd &poll_fd);

  public:
    Writer() = default;
    explicit Writer(std::string to_write) : buffer(std::move(to_write)) {}

    /**
     * Attempts to write the current buffer to the socket.
     * Automatically handles non-blocking constraints and partial writes.
     */
    SocketStatus write_to_socket(struct pollfd &pfd, SSL *ssl);

    /**
     * Swaps the current buffer for a new one and resets the write progress.
     */
    void change_buffer(std::string to_write);
};

/**
 * Handles non-blocking reads from a socket and extracts HTTP headers.
 */
class Reader {
  private:
    std::string buffer;
    bool is_header_present{false};
    size_t last_time_asked{0};

    SocketStatus read_using_https(struct pollfd &poll_fd, SSL *ssl);
    SocketStatus read_using_http(const struct pollfd &poll_fd);

    bool can_extract_header() noexcept;

  public:
    /**
     * Attempts to read available bytes from the socket into the internal
     * buffer.
     */
    SocketStatus read_from_socket(struct pollfd &poll_fd, SSL *ssl);

    /**
     * Clears internal state and extracts the remaining body data.
     * Used to transition from reading headers to streaming audio.
     */
    std::string restart() noexcept;

    /**
     * Checks if the double CRLF (\r\n\r\n) has arrived.
     * @return The headers string if found, otherwise `std::nullopt`
     */
    std::optional<std::string> try_to_fetch_header();

    const std::string &get_buffer() const { return buffer; }
};

} // namespace http

#endif