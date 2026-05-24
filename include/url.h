#ifndef URL_H
#define URL_H

#include <cstdint>
#include <string>
#include <string_view>

namespace url {

/**
 * Structure representing url address.
 */
struct Url {
    std::string address;
    uint16_t port;
    std::string path;
    bool is_https;
};

/**
 * @brief Parses `std::string_view` to `Url` struct.
 *
 * Accepts only `http` and `https` protocols. If no port is specified inside
 * `url` function assigns default port to the host (80 to http and 443 to
 * https).
 *
 * @throws `std::invalid_argument` is url does not meet the RFC standards
 */
[[nodiscard]] Url parse_url(std::string_view url);

} // namespace url

#endif