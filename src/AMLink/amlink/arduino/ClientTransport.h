#pragma once
// Transport over any Arduino `Client`: WiFiClient (ESP32 Wi-Fi and Ethernet, Pico W), EthernetClient (W5100/W5500),
// ... The network itself (Wi-Fi login, DHCP, cable) is set up by the sketch as usual.
#if defined(ARDUINO) || defined(AMLINK_ARDUINO_STUBS)

#include <Arduino.h>
#include <Client.h>

#include "../Transport.h"

namespace amlink {

class ClientTransport : public Transport {
public:
    explicit ClientTransport(Client& client) : client_(client) {}

    bool connect(const char* host, uint16_t port) override {
        client_.stop();
        return client_.connect(host, port) != 0;
    }
    bool connected() override { return client_.connected() != 0; }
    int available() override { return client_.available(); }
    size_t read(uint8_t* buffer, size_t length) override {
        int n = client_.read(buffer, length);
        return n > 0 ? static_cast<size_t>(n) : 0;
    }
    size_t write(const uint8_t* data, size_t length) override { return client_.write(data, length); }
    void stop() override { client_.stop(); }
    const char* kind() const override { return "tcp"; }

private:
    Client& client_;
};

}  // namespace amlink

#endif
