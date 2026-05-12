#include "program_args.h"

#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

using program_arguments::ProgramArguments;
using program_arguments::Verbosity;

using std::format;
using std::invalid_argument;
using std::logic_error;
using std::stoi;
using std::string;
using std::chrono::milliseconds;

milliseconds parse_timeout(const string &arg)
{
    static constexpr int MIN_TIME = 100, MAX_TIME = 100000;
    try {
        int timeout_val = stoi(arg);
        if (timeout_val < MIN_TIME || timeout_val > MAX_TIME) {
            throw invalid_argument(
                format("Option -t: timeout must be between {} and {} ms.",
                       MIN_TIME, MAX_TIME));
        }
        return milliseconds(timeout_val);
    } catch (const logic_error &) {
        throw invalid_argument("Option -t requires a numeric value.");
    }
}

Verbosity parse_verbosity(const string &arg)
{
    static constexpr int MIN_VERB = 0;
    static constexpr int MAX_VERB = 4;

    try {
        int v_val = stoi(arg);
        if (v_val < MIN_VERB || v_val > MAX_VERB) {
            throw invalid_argument(
                format("Option -v: verbosity must be between {} and {}.",
                       MIN_VERB, MAX_VERB));
        }
        return static_cast<Verbosity>(v_val);
    } catch (const logic_error &) {
        throw invalid_argument("Option -v requires a numeric value.");
    }
}

} // namespace

namespace program_arguments {

ProgramArguments get_args(int argc, char *argv[])
{
    ProgramArguments args{};
    int opt{};
    string url_str{};
    IpType type = IpType::Default;
    Verbosity verb = Verbosity::Warning;
    bool multiplex = true;
    milliseconds timeout{5000};

    opterr = 0;

    while ((opt = getopt(argc, argv, "u:mt:46v:q")) != -1) {
        switch (opt) {
        case 'u':
            url_str = optarg;
            break;

        case 'm':
            multiplex = true;
            break;

        case 't':
            timeout = parse_timeout(optarg);
            break;

        case '4':
            type = IpType::IPv4;
            break;

        case '6':
            type = IpType::IPv6;
            break;

        case 'v':
            verb = parse_verbosity(optarg);
            break;

        case 'q':
            verb = Verbosity::Quiet;
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

    if (!url_str.empty()) {
        throw invalid_argument("Missing required parameter: -u <url>");
    }

    return ProgramArguments{.url_address = url::parse_url(url_str),
                            .ip = type,
                            .verb = verb,
                            .multiplex = multiplex,
                            .timeout = timeout};
}

} // namespace program_arguments