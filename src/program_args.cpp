#include "program_args.h"

#include <format>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "url.h"
#include "verbosity.h"

namespace {
using std::format;
using std::invalid_argument;
using std::logic_error;
using std::stoi;
using std::string;

using log::parse_verbosity;
using log::Verbosity;

using program_arguments::ProgramArguments;

using url::parse_url;

int parse_timeout(const string &arg)
{
    static constexpr int MIN_TIME = 100;
    static constexpr int MAX_TIME = 100000;
    int timeout_val{};

    try {
        timeout_val = stoi(arg);
    } catch (const logic_error &) {
        throw invalid_argument("Option -t requires a numeric value.");
    }

    if (timeout_val < MIN_TIME || timeout_val > MAX_TIME) {
        throw invalid_argument(
            format("Option -t: timeout must be between {} and {} ms.", MIN_TIME,
                   MAX_TIME));
    }
    return timeout_val;
}

} // namespace

namespace program_arguments {

[[nodiscard]] ProgramArguments get_args(int argc, char *const argv[])
{
    ProgramArguments args{};
    int opt{};

    opterr = 0;

    while ((opt = getopt(argc, argv, "u:mt:46v:q")) != -1) {
        switch (opt) {
        case 'u':
            args.url_address = optarg;
            break;

        case 'm':
            args.is_multiplexing = true;
            break;

        case 't':
            args.timeout = parse_timeout(optarg);
            break;

        case '4':
            args.ip = IpType::IPv4;
            break;

        case '6':
            args.ip = IpType::IPv6;
            break;

        case 'v':
            args.verb = parse_verbosity(optarg);
            break;

        case 'q':
            args.verb = Verbosity::Quiet;
            break;

        case '?':
            if (optopt == 'u' || optopt == 't' || optopt == 'v') {
                throw invalid_argument(string("Option -") +
                                       static_cast<char>(optopt) +
                                       " requires an argument.");
            } else {
                throw invalid_argument(string("Unknown option -") +
                                       static_cast<char>(optopt) + ".");
            }

        default:
            throw invalid_argument("Error during argument parsing.");
        }
    }

    if (optind < argc) {
        throw invalid_argument("Unexpected positional arguments found.");
    }

    if (args.url_address.empty()) {
        throw invalid_argument("Missing required parameter: -u <url>");
    }

    return args;
}

} // namespace program_arguments