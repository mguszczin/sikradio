#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <chrono>
#include <string>

#include "url.h"

namespace program_arguments {

using url::Url;

enum class IpType { IPv4, IPv6, Default };

enum class Verbosity {
    Quiet,   // V0
    Error,   // V1
    Warning, // V2
    Info,    // V3
    Debug    // V4
};

struct ProgramArguments {

    Url url_address;

    IpType ip = IpType::Default;
    Verbosity verb = Verbosity::Warning;

    bool is_multiplexing = false;

    std::chrono::milliseconds timeout{5000};
};

[[nodiscard]] ProgramArguments get_args(int argc, char *argv[]);

} // namespace program_arguments

#endif