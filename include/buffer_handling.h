#ifndef WRITER_H
#define WRITER_H

#include <string>

#include <poll.h>

namespace http {

inline constexpr size_t PAGE_SIZE = 4096;

enum class SocketStatus { ConnectionClosed, Continuing, Finished, Error };

class Writer {
  private:
    std::string buffer;
    size_t bytes_sent{0};

    bool is_finished() const noexcept;

  public:
    Writer() = default;
    Writer(std::string to_write) : buffer(std::move(to_write)) {}

    SocketStatus write_to_socket(const struct pollfd &pfd);

    void restart() noexcept;

    void change_buffer(std::string to_write);
};

class Reader {
  private:
    std::string buffer;
    bool is_header_present{false};
    size_t last_time_asked{0};

  public:
    SocketStatus read_from_socket(const struct pollfd &pfd);

    std::string restart() noexcept;

    bool can_extract_header() noexcept;

    std::optional<std::string> try_to_fetch_header();

    const std::string &get_buffer() const { return buffer; }
};

} // namespace http

#endif