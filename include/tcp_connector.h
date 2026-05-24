#ifndef ASYNC_CONNECTOR
#define ASYNC_CONNECTOR

#include <netdb.h>
#include <utility>

#include "program_args.h"
#include "socket.h"
#include "url.h"

namespace client {

/**
 * Represents the current stage of the non-blocking connection process.
 */
enum class ConnectState {
    Found,     // Connection successfully established.
    NotFound,  // All IP addresses failed or timed out.
    Connecting // Connection started in the background (EINPROGRESS).
};

/**
 * Handles asynchronous (non-blocking) TCP connections.
 * It resolves URLs to IP addresses and iterates through them until
 * a successful connection is made.
 */
class TcpConnector {
  private:
    program_arguments::IpType ip_type;

    // Tracks the raw file descriptor of the socket currently trying to connect
    int given_fd = -1;

    struct addrinfo *addr_list_head = nullptr;
    struct addrinfo *current_addr = nullptr;

    void cleanup() noexcept;
    void set_up_addrinfo(struct addrinfo &hints) const noexcept;

  public:
    explicit TcpConnector(program_arguments::IpType ip);
    ~TcpConnector();

    // Prevent accidental copying of resources
    TcpConnector(const TcpConnector &) = delete;
    TcpConnector &operator=(const TcpConnector &) = delete;

    /**
     * Resolves the given URL's hostname into a list of usable IP addresses.
     * Must be called before start_looking().
     *
     * @throws `std::runtime_error` if the DNS resolution fails.
     */
    void connect_with_new_url(const url::Url &url);

    /**
     * Attempts to connect to the next available IP address in a non-blocking
     * way.
     * * Note on behavior:
     * - If a connection starts, it returns {ValidSocket, Connecting}. Ownership
     * of the socket is transferred to the caller.
     * - On subsequent calls, it checks the status of that pending connection.
     * If successful, it returns {EmptySocket, Found}. The caller is expected
     * to continue using the socket they were given in the 'Connecting' phase.
     */
    std::pair<http::Socket, ConnectState> start_looking() noexcept;
};

} // namespace client

#endif