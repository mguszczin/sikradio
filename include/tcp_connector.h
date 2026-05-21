#ifndef ASYNC_CONNECTOR
#define ASYNC_CONNECTOR

#include <netdb.h>
#include <utility>

#include "program_args.h"
#include "socket.h"
#include "url.h"

namespace client {

enum class ConnectState { Found, NotFound, Connecting };

class TcpConnector {
  private:
    void cleanup();
    void set_up_addrinfo(struct addrinfo &hints) const noexcept;

    program_arguments::IpType ip_type;
    int given_fd = -1;
    struct addrinfo *addr_list_head = nullptr;
    struct addrinfo *current_addr = nullptr;

  public:
    explicit TcpConnector(program_arguments::IpType ip);
    ~TcpConnector();

    TcpConnector(const TcpConnector &) = delete;
    TcpConnector &operator=(const TcpConnector &) = delete;

    void init(const url::Url &url);
    std::pair<http::Socket, ConnectState> start_looking();
};

} // namespace client

#endif