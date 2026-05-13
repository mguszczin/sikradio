#ifndef URL_H
#define URL_H

#include <cstdint>
#include <string>
#include <string_view>

namespace url {

struct Url {
    std::string address;
    uint16_t port;
    std::string path;
    bool is_https;
};

[[nodiscard]] Url parse_url(std::string_view url);

} // namespace url

#endif