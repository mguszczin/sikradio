#ifndef WRITER_H
#define WRITER_H

#include <string>

#include <poll.h>

namespace http {

inline constexpr size_t PAGE_SIZE = 4096;

enum class WriterStatus { ConnectionClosed, StillWriting, Finished, Error };

class Writer {
  private:
    std::string buffer;
    size_t bytes_sent{0};

    bool is_finished() const noexcept;

  public:
    Writer(std::string to_write) : buffer(std::move(to_write)) {}

    WriterStatus write_to_socket(const struct pollfd &pfd);

    void restart() noexcept;

    void change_buffer(std::string to_write);
};

} // namespace http

#endif