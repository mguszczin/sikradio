#ifndef WRITER_H
#define WRITER_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <poll.h>

#include <openssl/ssl.h>

namespace http {

inline constexpr size_t PAGE_SIZE = 4096;

enum class SocketStatus { ConnectionClosed, Continuing, Finished };

class Writer {
  private:
    std::string buffer;
    size_t bytes_sent{0};

    bool is_finished() const noexcept;
    SocketStatus write_using_https(struct pollfd &poll_fd, SSL *ssl);
    SocketStatus write_using_http(const struct pollfd &poll_fd);

  public:
    Writer() = default;
    Writer(std::string to_write) : buffer(std::move(to_write)) {}

    SocketStatus write_to_socket(struct pollfd &pfd, SSL *ssl);

    void restart() noexcept;

    void change_buffer(std::string to_write);
};

class Reader {
  private:
    std::string buffer;
    bool is_header_present{false};
    size_t last_time_asked{0};

    SocketStatus read_using_https(struct pollfd &poll_fd, SSL *ssl);
    SocketStatus read_using_http(const struct pollfd &poll_fd);

  public:
    SocketStatus read_from_socket(struct pollfd &poll_fd, SSL *ssl);

    std::string restart() noexcept;

    bool can_extract_header() noexcept;

    std::optional<std::string> try_to_fetch_header();

    const std::string &get_buffer() const { return buffer; }
};

enum class SslOperationResult { Success, NeedsRead, NeedsWrite, Closed };

SslOperationResult evaluate_ssl_error(SSL *ssl, int return_code);

} // namespace http

#endif