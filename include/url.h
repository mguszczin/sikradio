#ifndef URL_H
#define URL_H

#include <cstdint>
#include <string>

namespace url {

struct ParsedUrl {
    std::string address;
    uint16_t port;
    std::string path;
};

[[nodiscard]] ParsedUrl parse_url(const std::string &url);

} // namespace url

#endif