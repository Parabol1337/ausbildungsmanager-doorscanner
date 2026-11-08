#include "Env.h"

#include <stdio.h>
#include <string.h>

// Platform environments. Board support is explicit: a platform that is not listed here must provide its own
// Env (a class implementing amlink::Env) and define AMLINK_CUSTOM_ENV, so that no weak random source is used by accident.

#if defined(ARDUINO)

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#else
#include <esp_system.h>
#endif
#elif defined(ARDUINO_ARCH_RP2040)
#include <pico/unique_id.h>
#endif

namespace amlink {
namespace {

class ArduinoEnv : public Env {
public:
    uint32_t millis() override { return ::millis(); }

    void random(uint8_t* out, size_t length) override {
#if defined(ARDUINO_ARCH_ESP32)
        // Hardware RNG. Fully random while Wi-Fi or Bluetooth is running, which is the case for a networked device.
        esp_fill_random(out, length);
#elif defined(ARDUINO_ARCH_RP2040)
        // arduino-pico: hardware random from the ring oscillator and the system timer.
        for (size_t i = 0; i < length;) {
            uint32_t word = rp2040.hwrand32();
            for (int b = 0; b < 4 && i < length; b++, i++) out[i] = uint8_t(word >> (8 * b));
        }
#else
        (void)out;
        (void)length;
#endif
    }

    size_t hardwareId(char* out, size_t capacity) override {
#if defined(ARDUINO_ARCH_ESP32)
        uint64_t mac = ESP.getEfuseMac();
        int n = snprintf(out, capacity, "%04x%08x", unsigned(mac >> 32) & 0xffffu, unsigned(mac & 0xffffffffu));
        return n > 0 ? size_t(n) : 0;
#elif defined(ARDUINO_ARCH_RP2040)
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id);
        if (capacity < 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1) return 0;
        static const char digits[] = "0123456789abcdef";
        for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
            out[2 * i] = digits[id.id[i] >> 4];
            out[2 * i + 1] = digits[id.id[i] & 15];
        }
        out[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES] = 0;
        return 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES;
#else
        (void)out;
        (void)capacity;
        return 0;
#endif
    }

    const char* platform() const override {
#if defined(ARDUINO_ARCH_ESP32)
        return "esp32";
#elif defined(ARDUINO_ARCH_RP2040)
        return "rp2040";
#else
        return "other";
#endif
    }
};

}  // namespace

#if !defined(AMLINK_CUSTOM_ENV)
#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_RP2040)
#error "AMLink supports ESP32 and RP2040 out of the box. For other boards implement amlink::Env and define AMLINK_CUSTOM_ENV."
#endif
Env& defaultEnv() {
    static ArduinoEnv env;
    return env;
}
#endif

}  // namespace amlink

#else  // host (tests, simulators)

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace amlink {
namespace {

class HostEnv : public Env {
public:
    uint32_t millis() override {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return uint32_t(ts.tv_sec) * 1000u + uint32_t(ts.tv_nsec / 1000000);
    }

    void random(uint8_t* out, size_t length) override {
        int fd = open("/dev/urandom", O_RDONLY);
        size_t got = 0;
        while (fd >= 0 && got < length) {
            ssize_t n = read(fd, out + got, length - got);
            if (n <= 0) break;
            got += size_t(n);
        }
        if (fd >= 0) close(fd);
        if (got < length) memset(out + got, 0, length - got);  // Link::begin() refuses an all-zero secret
    }

    size_t hardwareId(char* out, size_t capacity) override {
        int n = snprintf(out, capacity, "%08x", unsigned(getpid()));
        return n > 0 ? size_t(n) : 0;
    }

    const char* platform() const override { return "host"; }
};

}  // namespace

Env& defaultEnv() {
    static HostEnv env;
    return env;
}

}  // namespace amlink

#endif
