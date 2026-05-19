#include <iostream>
#include <stdexcept>

#include "program_args.h"
#include "radio_client.h"
#include "url.h"
#include "verbosity.h"

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::string;

using program_arguments::get_args;
using program_arguments::IpType;
using program_arguments::ProgramArguments;

using logs::set_verbosity;

using client::RadioClient;

using url::parse_url;

int main(int argc, char *argv[])
{
    try {
        ProgramArguments args = get_args(argc, argv);
        set_verbosity(args.verb);

        logs::debug("Program Arguments : URL: '{}', IP Type: {}, Verbosity: "
                    "{}, Multiplexing: {}, Timeout: {}ms",
                    args.url_address.empty() ? "none" : args.url_address,
                    static_cast<int>(args.ip), static_cast<int>(args.verb),
                    args.is_multiplexing ? "true" : "false", args.timeout);

        RadioClient client{args.is_multiplexing, args.timeout, args.ip,
                           parse_url(args.url_address)};
        client.start();
    } catch (std::exception &e) {
        logs::error("{}", e.what());
        return 1;
    }
    return 0;
}
