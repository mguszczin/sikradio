#ifndef RADIO_CLIENT_H
#define RADIO_CLIENT_H

#include "program_args.h"

namespace client {

using program_arguments::ProgramArguments;

class RadioClient {
  private:
    ProgramArguments radio_args;

  public:
    RadioClient(ProgramArguments args) : radio_args(std::move(args)) {};

    // void start();
};

} // namespace client

#endif