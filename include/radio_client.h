#ifndef RADIO_CLIENT_H
#define RADIO_CLIENT_H

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "buffer_handling.h"
#include "printer.h"
#include "program_args.h"
#include "socket.h"
#include "tcp_connector.h"
#include "url.h"

namespace client {

class RadioClient {
  private:
    static constexpr size_t LISTENING_POINTS = 2;
    static constexpr std::string_view QUIT_MESSAGE = "quit\n";

    enum class ClientModes {
        Connecting,
        TlsHandshake,
        SendingData,
        ReadingHeaders,
        ReadingBody,
    };

    const bool is_multiplexing;
    const int timeout;
    std::map<std::string, std::string> cur_cookies;
    program_arguments::IpType ip;

    SSL_CTX *ssl_ctx = nullptr;
    SSL *ssl = nullptr;

    TcpConnector connector;
    http::Reader reader;
    http::Writer writer;
    Printer printer;

    const url::Url orginal_url;
    url::Url cur_url;

    ClientModes mode;
    std::chrono::milliseconds last_time_connected{0};

    http::Socket server_socket;

    void start_sending_data(struct pollfd &poll_fd);

    void handle_succesful_connection(struct pollfd &poll_fd);
    /**
     * Establishes connection with server under `cur_url`
     * and returns socket descriptor to that url.
     *
     * @throws `std::runtime_error` if function can't connect
     * to given url.
     */
    void establish_connection(struct pollfd &poll_fd);

    void handle_new_headers(struct pollfd &poll, const std::string &headers);

    bool handle_sending_request(struct pollfd &poll);

    bool handle_reading_headers(struct pollfd &poll);

    bool handle_reading_body(struct pollfd &poll);

    bool handle_server_comunication(struct pollfd &poll);

    bool handle_tls_handshake(struct pollfd &poll_fd);

    void handle_connecting_to_socket(struct pollfd &poll_fd);

    bool handle_user_input(struct pollfd &poll);

    void handle_timeout(struct pollfd &poll_fd);

    int calc_timeout(std::chrono::_V2::steady_clock::time_point
                         last_server_activity) const noexcept;

  public:
    RadioClient(bool is_multiplexing, int timeout, program_arguments::IpType ip,
                url::Url url)
        : is_multiplexing(is_multiplexing), timeout(timeout), ip(ip),
          connector(ip), reader(), writer(), printer(), orginal_url(url),
          cur_url(std::move(url))
    {
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    };

    RadioClient(const RadioClient &) = delete;
    RadioClient &operator=(const RadioClient &) = delete;

    ~RadioClient()
    {
        if (ssl)
            SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
    }

    void start();
};

} // namespace client

#endif