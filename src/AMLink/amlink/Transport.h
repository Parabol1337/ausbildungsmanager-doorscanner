#pragma once
#include "Config.h"

namespace amlink {

/**
 * The byte pipe to the server. AMLink never talks to hardware directly, so the same library runs on a
 * Wi-Fi ESP32, on a W5500 Ethernet board or behind a serial TCP bridge such as the CH9120.
 *
 * Implementations for Arduino are in arduino/ClientTransport.h and arduino/SerialBridgeTransport.h.
 * All methods must be non-blocking (connect() may block for a few seconds).
 */
class Transport {
public:
    virtual ~Transport() {}

    /** Opens the connection. Bridges that connect on their own just report connected(). */
    virtual bool connect(const char* host, uint16_t port) = 0;
    virtual bool connected() = 0;
    /** Bytes ready to read (0 if none). */
    virtual int available() = 0;
    /** Reads up to `length` bytes, returns the number read (0 if none). */
    virtual size_t read(uint8_t* buffer, size_t length) = 0;
    /** Writes up to `length` bytes, returns the number accepted (may be less than `length`). */
    virtual size_t write(const uint8_t* data, size_t length) = 0;
    /** Closes the connection. */
    virtual void stop() = 0;
    /** Last resort for a stuck link (e.g. reset a bridge chip). Default: nothing. */
    virtual void reset() {}
    /**
     * True if the connection is established by something else (a bridge chip) and the server speaks first as soon as
     * it exists. The library then serves such a connection immediately and never delays it: the server gives up on a
     * connection that does not log in within 15 seconds.
     */
    virtual bool autoConnects() const { return false; }
    /** "tcp" or "serial-bridge": reported to the server for the hardware overview. */
    virtual const char* kind() const { return "tcp"; }
};

}  // namespace amlink
