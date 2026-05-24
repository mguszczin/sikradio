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

/**
 * The main client responsible for managing the connection to the radio server.
 * It handles standard TCP and TLS connections, HTTP redirects, Icecast metadata
 * multiplexing, and the main event loop (poll) to process incoming audio
 * streams.
 */
class RadioClient {
  private:
    // We listen to exactly 2 file descriptors: stdin (for "quit") and the
    // server socket.
    static constexpr size_t LISTENING_POINTS = 2;
    static constexpr std::string_view QUIT_MESSAGE = "quit\n";

    /**
     * Represents the internal state machine of the client connection.
     */
    enum class ClientModes {
        Connecting,     // Waiting for the TCP connection to establish
        TlsHandshake,   // TCP connected, now negotiating SSL/TLS
        SendingData,    // Sending the HTTP GET request
        ReadingHeaders, // Waiting for the server's HTTP response headers
        ReadingBody, // Connection fully established, streaming audio/metadata
    };

    const bool is_multiplexing;
    const int timeout;
    std::map<std::string, std::string> cur_cookies;
    program_arguments::IpType ip;

    // OpenSSL context and session
    SSL_CTX *ssl_ctx = nullptr;
    SSL *ssl = nullptr;

    // Helpers for specific networking and processing tasks
    TcpConnector connector;
    http::Reader reader;
    http::Writer writer;
    Printer printer;

    const url::Url
        orginal_url; // Saved fallback in case of redirects or reconnects
    url::Url cur_url;

    ClientModes mode;
    std::chrono::milliseconds last_time_connected{0};

    http::Socket server_socket;

    /**
     * Transitions the client to the SendingData state and prepares the HTTP
     * request buffer.
     */
    void start_sending_data(struct pollfd &poll_fd);

    /**
     * Called when a TCP connection is successfully made. It decides whether to
     * start sending data immediately (HTTP) or initiate a TLS handshake
     * (HTTPS).
     */
    void handle_succesful_connection(struct pollfd &poll_fd);

    /**
     * Resolves the current URL and initiates a non-blocking TCP connection.
     *
     * @throws `std::runtime_error` if DNS resolution or connection initiation
     * fails.
     */
    void establish_connection(struct pollfd &poll_fd);

    /**
     * Parses the server's HTTP response. Handles 3XX redirects and validates
     * Icecast metadata intervals if multiplexing was requested.
     */
    void handle_new_headers(struct pollfd &poll, const std::string &headers);

    /**
     * Pushes the queued HTTP request buffer into the socket.
     * @return True if the client should exit, false to continue.
     */
    bool handle_sending_request(struct pollfd &poll);

    /**
     * Reads incoming bytes from the socket until the double CRLF (end of
     * headers) is found.
     * @return True if the connection was unexpectedly closed.
     */
    bool handle_reading_headers(struct pollfd &poll);

    /**
     * Reads the raw audio stream (and metadata) and forwards it to the printer.
     * @return True if the server ended the stream.
     */
    bool handle_reading_body(struct pollfd &poll);

    /**
     * Core router for socket events. Dispatches to the correct handler based on
     * `ClientModes`.
     */
    bool handle_server_comunication(struct pollfd &poll);

    /**
     * Negotiates the TLS connection state.
     */
    bool handle_tls_handshake(struct pollfd &poll_fd);

    /**
     * Periodically checks the non-blocking socket to see if the TCP connection
     * has completed.
     */
    void handle_connecting_to_socket(struct pollfd &poll_fd);

    /**
     * Checks standard input for user command ("quit").
     * @return True if the user requested to quit the application.
     */
    bool handle_user_input(struct pollfd &poll);

    /**
     * Triggers a complete connection reset to the original URL when the server
     * times out.
     */
    void handle_timeout(struct pollfd &poll_fd);

    /**
     * Calculates the remaining time before the connection is considered timed
     * out.
     * @return Milliseconds remaining until timeout (can be negative if already
     * timed out).
     */
    int calc_timeout(std::chrono::steady_clock::time_point last_server_activity)
        const noexcept;

  public:
    /**
     * Initializes the client with configuration and sets up the OpenSSL
     * context.
     */
    RadioClient(bool is_multiplexing, int timeout, program_arguments::IpType ip,
                url::Url url)
        : is_multiplexing(is_multiplexing), timeout(timeout), ip(ip),
          connector(ip), reader(), writer(), printer(), orginal_url(url),
          cur_url(std::move(url))
    {
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    };

    // Prevent accidental copying which would double-free SSL contexts or
    // sockets
    RadioClient(const RadioClient &) = delete;
    RadioClient &operator=(const RadioClient &) = delete;

    ~RadioClient()
    {
        if (ssl) {
            SSL_free(ssl);
        }
        SSL_CTX_free(ssl_ctx);
    }

    /**
     * Starts the main poll() loop. Blocks until the user quits, an
     * unrecoverable error occurs, or the server closes the stream.
     */
    void start();
};

} // namespace client

#endif