# SIK Radio

A C++ internet radio client built for a computer networking assignment. The task was to receive an audio stream over TCP using IPv4 or IPv6, optionally separate text metadata from the stream, and pass the audio to an external player.

The client supports HTTP and HTTPS streams, redirects, Icecast metadata, and reconnection after a period without data. It writes audio unchanged to standard output and metadata to standard error.

## Build and run

```sh
make
./sikradio -u https://example.com/stream | mpv --really-quiet -
```

Use `-m` to request metadata, `-4` or `-6` to select an IP version, and `-t` to set the reconnect timeout in milliseconds. Type `quit` and press Enter to stop the client.
