#pragma once
#include "Config.h"

namespace amlink {

/** Everything platform specific that is not a network or a store: time, randomness, identity. */
class Env {
public:
    virtual ~Env() {}
    virtual uint32_t millis() = 0;
    /** Cryptographically strong random bytes (the device secret and nonces depend on it). */
    virtual void random(uint8_t* out, size_t length) = 0;
    /** Unique hardware id as lower-case hex (e.g. MAC or chip id); returns the length, 0 if unknown. */
    virtual size_t hardwareId(char* out, size_t capacity) = 0;
    /** "rp2040", "esp32", "esp8266", "android", "host" or "other". */
    virtual const char* platform() const = 0;
};

/** The environment of the board the library is compiled for (Arduino cores, or the host for tests). */
Env& defaultEnv();

}  // namespace amlink
