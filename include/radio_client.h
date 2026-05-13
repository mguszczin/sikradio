#ifndef RADIO_CLIENT_H
#define RADIO_CLIENT_H

#include "program_args.h"
#include "url.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace client {

using program_arguments::ProgramArguments;
using url::Url;

class RadioClient {
  private:
    ProgramArguments radio_args;

    void set_up_addrinfo(struct addrinfo &hints) const;

    /**
     * Establishes connection with server under `cur_url`
     * and returns socket descriptor to that url.
     *
     * @throws `std::runtime_error` if function can't connect
     * to given url.
     */
    int establish_connection(const Url &url) const;

    int find_radio_server(const Url &url) const;

  public:
    RadioClient(ProgramArguments args) : radio_args(std::move(args)) {};

    void start();
};

} // namespace client

#endif