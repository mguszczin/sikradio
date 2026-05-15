#ifndef RADIO_CLIENT_H
#define RADIO_CLIENT_H

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "buffer_handling.h"
#include "program_args.h"
#include "url.h"

namespace client {

using program_arguments::ProgramArguments;

using url::Url;

using http::Reader;
using http::Writer;

class RadioClient {
  private:
    static constexpr size_t LISTENING_POINTS = 2;
    static constexpr std::string QUIT_MESSAGE = "quit";

    enum class ClientModes { SendingData, ReadingData };

    ProgramArguments radio_args;

    Reader reader;
    Writer writer;

    ClientModes mode;
    Url cur_url;

    void handle_new_headers(struct pollfd &poll, const std::string &headers);

    void handle_timeout();

    bool handle_user_input(const struct pollfd &poll);

    bool handle_server_comunication(struct pollfd &poll);

    void set_up_addrinfo(struct addrinfo &hints) const;

    /**
     * Establishes connection with server under `cur_url`
     * and returns socket descriptor to that url.
     *
     * @throws `std::runtime_error` if function can't connect
     * to given url.
     */
    int establish_connection(const Url &url) const;

  public:
    RadioClient(ProgramArguments args)
        : radio_args(std::move(args)), reader(), writer(),
          cur_url(radio_args.url_address) {};

    void start();
};

} // namespace client

#endif