#include "printer.h"
#include <algorithm>
#include <iostream>

namespace client {

namespace {
using std::cerr;
using std::cout;
using std::endl;
using std::min;
using std::string;
} // namespace

void Printer::process_audio(const string &buffer, size_t &i)
{
    size_t target_meta = static_cast<size_t>(meta_int);
    size_t bytes_to_write =
        min(buffer.size() - i, target_meta - audio_bytes_read);

    cout.write(buffer.data() + i, bytes_to_write);

    audio_bytes_read += bytes_to_write;
    i += bytes_to_write;

    if (audio_bytes_read == target_meta) {
        expecting_meta_length = true;
    }
}

void Printer::process_meta_length(const string &buffer, size_t &i)
{
    unsigned char length_byte = static_cast<unsigned char>(buffer[i]);
    meta_bytes_to_read = length_byte * 16;

    expecting_meta_length = false;
    current_metadata.clear();
    i++;

    if (meta_bytes_to_read == 0) {
        audio_bytes_read = 0;
    }
}

void Printer::process_metadata(const string &buffer, size_t &i)
{
    size_t bytes_to_read = min(buffer.size() - i, meta_bytes_to_read);

    current_metadata.append(buffer, i, bytes_to_read);

    meta_bytes_to_read -= bytes_to_read;
    i += bytes_to_read;

    if (meta_bytes_to_read == 0) {
        while (!current_metadata.empty() && current_metadata.back() == '\0') {
            current_metadata.pop_back();
        }

        if (!current_metadata.empty()) {
            cerr << current_metadata << endl;
        }

        audio_bytes_read = 0;
    }
}

bool Printer::print(const string &buffer)
{
    if (meta_int <= 0) {
        cout.write(buffer.data(), buffer.size());
        return !cout.fail();
    }

    size_t i = 0;
    size_t target_meta = static_cast<size_t>(meta_int);

    while (i < buffer.size()) {
        if (audio_bytes_read < target_meta) {
            process_audio(buffer, i);
        } else if (expecting_meta_length) {
            process_meta_length(buffer, i);
        } else if (meta_bytes_to_read > 0) {
            process_metadata(buffer, i);
        }
    }

    cout.flush();
    return cout.good();
}

} // namespace client