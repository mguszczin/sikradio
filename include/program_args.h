#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <string>

#include "verbosity.h"

namespace program_arguments {

/**
 * IP version preference.
 * Controlled by the -4 or -6 command-line flags.
 * 'Default' means the system will decide based on the URL.
 */
enum class IpType { IPv4, IPv6, Default };

/**
 * Holds all configuration passed from the command line.
 * Safe default values are automatically set for optional flags.
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
 * @brief Parses standard command-line arguments into a structured object.

 * @throws std::invalid_argument if:
 * - The required URL (-u) is missing or malformed.
 * - An unknown flag is passed or a flag is missing its required value.
 * - The timeout (-t) is not a number or falls outside MIN_TIME and MAX_TIME.
 */
[[nodiscard]] ProgramArguments get_args(int argc, char *const argv[]);

} // namespace program_arguments

#endif