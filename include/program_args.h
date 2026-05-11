#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <chrono>
#include <string>

namespace program_arguments {

#include <string>
#include <chrono>

struct ProgramArguments {
    enum class IpType {
        IPv4,
        IPv6,
        Default
    };

    enum class Verbosity {
        Quiet,   // V0
        Error,   // V1
        Warning, // V2
        Info,    // V3
        Debug    // V4
    };

    std::string url_address;
    
    IpType ip = IpType::Default;
    Verbosity verb = Verbosity::Warning;
    
    bool multiplex = false;

    std::chrono::milliseconds timeout{5000};
};

// [[nodiscard]] ProgramArguments get_args(int argc, char *argv[]);

}

#endif