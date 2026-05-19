#include "verbosity.h"

#include <stdexcept>
#include <string>

namespace {
using std::invalid_argument;
using std::stoi;
using std::string;
} // namespace

namespace log {

Verbosity parse_verbosity(const string &arg)
{
    static constexpr int MIN_VERB = 0;
    static constexpr int MAX_VERB = 4;
    int v_val{};

    try {
        v_val = stoi(arg);
    } catch (const logic_error &) {
        throw invalid_argument("Option -v requires a numeric value.");
    }

    if (v_val < MIN_VERB || v_val > MAX_VERB) {
        throw invalid_argument(
            format("Option -v: verbosity must be between {} and {}.", MIN_VERB,
                   MAX_VERB));
    }
    return static_cast<Verbosity>(v_val);
}

} // namespace log
