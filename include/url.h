#ifndef URL_H
#define URL_H

#include <cstdint>
#include <string>

namespace url {

struct ParsedUrl {

    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t HTTPS_PORT = 443;

    bool is_https = false;
    std::string domain = "";
    uint16_t port;
    std::string resource;
};

// [[nodiscard]] ParsedUrl parse_url(std::string &url);

} // namespace url

#endif