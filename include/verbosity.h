#ifndef VERBOSITY_H
#define VERBOSITY_H

#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace logs {

enum class Verbosity {
    Quiet,         // V0
    ServerInfo,    // V1
    Error,         // V2
    SystemWarning, // V3
    Debug          // V4
};

inline Verbosity current_verbosity = Verbosity::Error;

inline void set_verbosity(Verbosity v)
{
    static bool is_set = false;
    if (is_set) {
        throw std::logic_error("Verbosity level can only be set once.");
    }
    current_verbosity = v;
    is_set = true;
}

template <typename... Args>
inline void error(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Error)
        std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
inline void warning(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::SystemWarning)
        std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
inline void server_info(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::ServerInfo)
        std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
inline void debug(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Debug)
        std::cerr << "[DEBUG] " << std::format(fmt, std::forward<Args>(args)...)
                  << std::endl;
}

/**
 * @brief Function converts string argument to Verbosity enum.
 *
 * @throws `std::invalid_argument` if given parameter is not numeric or is not
 * in range from 0 to 4.
 */
Verbosity parse_verbosity(const std::string &arg);

} // namespace logs

#endif