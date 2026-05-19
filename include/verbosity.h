#ifndef VERBOSITY_H
#define VERBOSITY_H

#include <format>
#include <iostream>
#include <utility>

namespace log {

enum class Verbosity {
    Quiet,   // V0
    Error,   // V1
    Warning, // V2
    Info,    // V3
    Debug    // V4
};

inline Verbosity current_verbosity = Verbosity::Info;

inline void set_verbosity(Verbosity v) noexcept { current_verbosity = v; }

template <typename... Args>
inline void error(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Error)
        std::cerr << "[ERROR] " << std::format(fmt, std::forward<Args>(args)...)
                  << std::endl;
}

template <typename... Args>
inline void warning(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Warning)
        std::cerr << "[WARNING]  "
                  << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

template <typename... Args>
inline void info(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Info)
        std::cout << "[INFO]  " << std::format(fmt, std::forward<Args>(args)...)
                  << std::endl;
}

template <typename... Args>
inline void debug(std::format_string<Args...> fmt, Args &&...args)
{
    if (current_verbosity >= Verbosity::Debug)
        std::cout << "[DEBUG] " << std::format(fmt, std::forward<Args>(args)...)
                  << std::endl;
}

} // namespace log

#endif