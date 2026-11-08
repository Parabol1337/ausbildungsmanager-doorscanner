#pragma once
#include <stdarg.h>

#include "Arduino.h"

class String {
public:
    String() : s_() {}
    String(const char* c) { snprintf(s_, sizeof(s_), "%s", c); }
    String(int v) { snprintf(s_, sizeof(s_), "%d", v); }
    String(unsigned long v) { snprintf(s_, sizeof(s_), "%lu", v); }
    unsigned length() const { return unsigned(strlen(s_)); }
    const char* c_str() const { return s_; }
    String& operator+=(const String& o) { strncat(s_, o.s_, sizeof(s_) - strlen(s_) - 1); return *this; }
    String& operator=(const char* c) { snprintf(s_, sizeof(s_), "%s", c); return *this; }
private:
    char s_[64];
};

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* data, size_t length) {
        size_t n = 0;
        while (n < length && write(data[n])) n++;
        return n;
    }
    size_t print(const char* t) { return write(reinterpret_cast<const uint8_t*>(t), strlen(t)); }
    size_t println(const char* t) { return print(t) + write('\n'); }
    size_t print(const String& t) { return print(t.c_str()); }
    size_t println(const String& t) { return println(t.c_str()); }
    size_t println(int v) { return println(String(v)); }
    size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3))) {
        char buffer[160];
        va_list args;
        va_start(args, format);
        int n = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        return n > 0 ? print(buffer) : 0;
    }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
};

class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    size_t write(uint8_t) override { return 1; }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
};
extern HardwareSerial& Serial;
extern HardwareSerial& Serial1;
extern HardwareSerial& Serial2;
