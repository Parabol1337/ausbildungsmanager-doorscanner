#pragma once
#include "WiFiClient.h"
#define WIFI_STA 1
class WiFiClass {
public:
    void mode(int) {}
    void setAutoReconnect(bool) {}
    void begin(const char*, const char*) {}
};
extern WiFiClass WiFi;
