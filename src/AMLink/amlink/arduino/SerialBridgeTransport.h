#pragma once
// Transport for devices whose TCP connection is held by a serial bridge chip, such as the WCH CH9120 on the
// Waveshare RP2040-ETH boards. The chip connects to the server on its own (it is configured with its server address
// by its own settings, see CH9120_init() in the existing firmware); this transport only reads and writes the serial
// port and watches the chip's "not connected" pin.
#if defined(ARDUINO) || defined(AMLINK_ARDUINO_STUBS)

#include <Arduino.h>
#include <Stream.h>

#include "../Transport.h"

namespace amlink {

class SerialBridgeTransport : public Transport {
public:
    /**
     * @param serial          serial port connected to the bridge
     * @param disconnectedPin input pin that shows the connection state (CH9120: TCPCS, high = not connected)
     * @param highMeansDown   true if a HIGH level means "not connected" (CH9120)
     * @param resetBridge     optional: restarts the chip; called when the connection looks stuck
     */
    SerialBridgeTransport(Stream& serial, int disconnectedPin, bool highMeansDown = true, void (*resetBridge)() = nullptr)
        : serial_(serial), pin_(disconnectedPin), highMeansDown_(highMeansDown), reset_(resetBridge) {
        pinMode(pin_, INPUT);
    }

    /**
     * The chip connects by itself: report whether it is connected right now. Nothing is thrown away here - the
     * server's first line may already be waiting in the receive buffer.
     */
    bool connect(const char*, uint16_t) override { return connected(); }
    bool autoConnects() const override { return true; }
    bool connected() override { return (digitalRead(pin_) == HIGH) != highMeansDown_; }
    int available() override { return serial_.available(); }
    size_t read(uint8_t* buffer, size_t length) override {
        size_t n = 0;
        while (n < length && serial_.available() > 0) {
            int c = serial_.read();
            if (c < 0) break;
            buffer[n++] = static_cast<uint8_t>(c);
        }
        return n;
    }
    size_t write(const uint8_t* data, size_t length) override { return serial_.write(data, length); }
    /**
     * The library gives the connection up (or it was lost): what is left in the receive buffer belongs to the old
     * connection and is thrown away so that it cannot be mistaken for the start of the next one. The chip itself stays
     * connected until the server closes the connection or the chip is reset (see `resetBridge`).
     */
    void stop() override { drain(); }
    void reset() override {
        if (reset_) reset_();
    }
    const char* kind() const override { return "serial-bridge"; }

private:
    void drain() {
        while (serial_.available() > 0) serial_.read();
    }
    Stream& serial_;
    int pin_;
    bool highMeansDown_;
    void (*reset_)();
};

}  // namespace amlink

#endif
