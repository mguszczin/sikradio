#ifndef RADIO_CLIENT_H
#define RADIO_CLIENT_H

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
#include "url.h"

namespace client {

using program_arguments::ProgramArguments;

using url::Url;

using http::Reader;
using http::Socket;
using http::Writer;

using client::Printer;

class RadioClient {
  private:
    static constexpr size_t LISTENING_POINTS = 2;
    static constexpr std::string QUIT_MESSAGE = "quit";

    enum class ClientModes {
        SendingData,
        ReadingHeaders,
        ReadingBody,
        TlsHandshake
    };

    ProgramArguments radio_args;

    SSL_CTX *ssl_ctx = nullptr;
    SSL *ssl = nullptr;

    Reader reader;
    Writer writer;
    Printer printer;

    ClientModes mode;
    Url cur_url;

    Socket server_socket;

    void handle_new_headers(struct pollfd &poll, const std::string &headers);

    bool handle_sending_request(struct pollfd &poll);

    bool handle_reading_headers(struct pollfd &poll);

    bool handle_reading_body(struct pollfd &poll);

    bool handle_server_comunication(struct pollfd &poll);

    bool handle_user_input(const struct pollfd &poll);

    bool handle_tls_handshake(struct pollfd &poll_fd);

    void handle_timeout();

    int connect_to_socket(const struct addrinfo *res) const noexcept;

    void set_up_addrinfo(struct addrinfo &hints) const;

    /**
     * Establishes connection with server under `cur_url`
     * and returns socket descriptor to that url.
     *
     * @throws `std::runtime_error` if function can't connect
     * to given url.
     */
    void establish_connection(const Url &url, struct pollfd &poll_fd);

  public:
    RadioClient(ProgramArguments args)
        : radio_args(std::move(args)), reader(), writer(), printer(),
          cur_url(radio_args.url_address)
    {
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_default_verify_paths(ctx);
    };

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