#ifndef SOCKET_H
#define SOCKET_H

#include <unistd.h>
#include <verbosity.h>

namespace http {

/**
 * Socket class that utilizes RAII to ensure proper socket deallocation.
 */
class Socket {
  private:
    int fd;

  public:
    explicit Socket(int fd = -1) : fd(fd) {}

    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    Socket(Socket &&other) noexcept : fd(other.fd) { other.fd = -1; }

    Socket &operator=(Socket &&other) noexcept
    {
        if (this != &other && other.fd != fd) {
            if (fd >= 0) {
                logs::debug("closing desc number {} in assignment", fd);
                close(fd);
            }
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }

    ~Socket()
    {
        if (fd >= 0) {
            logs::debug("closing desc number {} in desc", fd);
            close(fd);
        }
    }

    operator int() const { return fd; }
    bool is_valid() const noexcept { return fd >= 0; }
};

} // namespace http

#endif