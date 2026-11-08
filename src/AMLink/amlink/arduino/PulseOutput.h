#pragma once
// A digital output that is switched on for a given time without blocking the main loop (a door opener, a relay).
// delay() must not be used for this: while the sketch waits, AMLink cannot answer pings and the server drops the device.
#if defined(ARDUINO) || defined(AMLINK_ARDUINO_STUBS)

#include <Arduino.h>

namespace amlink {

class PulseOutput {
public:
    /** `activeHigh`: true if the output is HIGH while "on" (most relay boards: check yours). */
    explicit PulseOutput(int pin, bool activeHigh = true) : pin_(pin), activeHigh_(activeHigh), on_(false), until_(0) {}

    /** Call once in setup(): configures the pin and makes sure the output is off. */
    void begin() {
        pinMode(pin_, OUTPUT);
        set(false);
    }

    /** Switches on for `milliseconds`. Calling it again while on restarts the time. */
    void start(uint32_t milliseconds) {
        until_ = millis() + milliseconds;
        set(true);
    }

    /** Switches off right away. */
    void stop() { set(false); }

    /** Call in loop(). Returns true exactly once, when the pulse has just ended. */
    bool update() {
        if (on_ && static_cast<int32_t>(millis() - until_) >= 0) {
            set(false);
            return true;
        }
        return false;
    }

    bool active() const { return on_; }

private:
    void set(bool on) {
        on_ = on;
        digitalWrite(pin_, (on == activeHigh_) ? HIGH : LOW);
    }
    int pin_;
    bool activeHigh_, on_;
    uint32_t until_;
};

}  // namespace amlink

#endif
