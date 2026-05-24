#ifndef URL_H
#define URL_H

#include <cstdint>
#include <string>
#include <string_view>

namespace url {

/**
 * Holds the extracted components of a successfully parsed URL.
 */
struct Url {
    std::string address;
    uint16_t port;
    std::string path;
    bool is_https;
};

/**
 * @brief Parses a raw URL string into its structural components.
 *
 *
 * @throws `std::invalid_argument` if:
 * - The protocol is missing or anything other than "http://" or "https://".
 * - The host address is missing (e.g., "http:///path").
 * - The IPv6 address is malformed (missing brackets or misplaced colons).
 * - The custom port provided is not a valid number.
 */
[[nodiscard]] Url parse_url(std::string_view url);

} // namespace url

#endif