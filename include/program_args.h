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
    log::Verbosity verb = log::Verbosity::SystemWarning;

    bool is_multiplexing = false;

    int timeout{5000};
};

/**
 * @brief Parses command-line arguments.
 *
 * @throws `std::invalid_argument` If the URL (-u) is missing or arguments are
 * invalid.
 */
[[nodiscard]] ProgramArguments get_args(int argc, char *const argv[]);

} // namespace program_arguments

#endif