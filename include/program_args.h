#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <string>

#include "url.h"
#include "verbosity.h"

namespace program_arguments {

/* All possible options for specifing ip type. */
enum class IpType { IPv4, IPv6, Default };

/**
 * Structure representing program arguments with default values already set.
 */
struct ProgramArguments {

    std::string url_address;

    IpType ip = IpType::Default;
    logs::Verbosity verb = logs::Verbosity::SystemWarning;

    bool is_multiplexing = false;

    static constexpr int MIN_TIME = 100;
    static constexpr int MAX_TIME = 100000;
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