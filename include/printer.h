#ifndef PRINTER_H
#define PRINTER_H

#include <cstdint>
#include <string>

namespace client {

class Printer {
  private:
    int meta_int;
    size_t audio_bytes_read;
    size_t meta_bytes_to_read;
    bool expecting_meta_length;
    std::string current_metadata;

    void process_audio(const std::string &buffer, size_t &i) noexcept;
    void process_meta_length(const std::string &buffer, size_t &i) noexcept;
    void process_metadata(const std::string &buffer, size_t &i);

  public:
    Printer(int metaint = -1)
        : meta_int{metaint}, audio_bytes_read{0}, meta_bytes_to_read{0},
          expecting_meta_length{false}
    {
    }

    void set_metaint(int c) noexcept { meta_int = c; }

    /**
     * Prints the buffer to stdout (audio) and stderr (metadata),
     * taking metaint into account.
     */
    void print(const std::string &buffer);

    void reset() noexcept
    {
        audio_bytes_read = 0;
        meta_bytes_to_read = 0;
        expecting_meta_length = false;
        current_metadata.clear();
    }
};

} // namespace client

#endif