#ifndef PRINTER_H
#define PRINTER_H

#include <cstdint>
#include <string>
#include <string_view>

namespace client {

/**
 * Handles the separation and output of audio and metadata streams.
 * * If multiplexing is disabled (metaint <= 0), it dumps all data to stdout.
 * If multiplexing is enabled (Icecast/SHOUTcast protocol), it counts the
 * bytes, extracts the metadata blocks, prints them to stderr, and passes
 * the pure audio to stdout.
 */
class Printer {
  private:
    int meta_int;
    size_t audio_bytes_read;
    size_t meta_bytes_to_read;
    bool expecting_meta_length;
    std::string current_metadata;

    void process_audio(std::string_view buffer, size_t &i) noexcept;
    void process_meta_length(std::string_view buffer, size_t &i) noexcept;
    void process_metadata(std::string_view buffer, size_t &i);

  public:
    Printer(int metaint = -1)
        : meta_int{metaint}, audio_bytes_read{0}, meta_bytes_to_read{0},
          expecting_meta_length{false}
    {
    }

    /**
     * Updates the multiplexing interval (metaint).
     * Passing a value <= 0 disables metadata parsing.
     */
    void set_metaint(int c) noexcept { meta_int = c; }

    /**
     * Processes a chunk of data received from the network.
     * Audio is written to standard output (stdout).
     * Metadata is written to standard error (stderr).
     * * @throws std::runtime_error if stdout pipe breaks.
     */
    void print(std::string_view buffer);

    /**
     * Resets the internal state machine.
     * Should be called if the stream restarts or reconnects.
     */
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