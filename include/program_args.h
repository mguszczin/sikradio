#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <string>

#include "url.h"
#include "verbosity.h"

namespace program_arguments {

enum class IpType { IPv4, IPv6, Default };

struct ProgramArguments {

    std::string url_address;

    IpType ip = IpType::Default;
    log::Verbosity verb = log::Verbosity::Warning;

    bool is_multiplexing = false;

    int timeout{5000};
};

/**
 * Function reads arguments from `argv`. Assumes the arguments are in the same
 * format as program arguments.
 *
 * @throws `std::invalid_argument` if no url (-u flag) is specified or if flags
 * are not used correctly.
 */
[[nodiscard]] ProgramArguments get_args(int argc, char *const argv[]);

} // namespace program_arguments

#endif