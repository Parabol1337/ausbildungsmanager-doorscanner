#pragma once
#include "Stream.h"

class Client : public Stream {
public:
    virtual int connect(const char* host, uint16_t port) = 0;
    virtual size_t write(uint8_t b) = 0;
    virtual size_t write(const uint8_t* buf, size_t size) = 0;
    virtual int read(uint8_t* buf, size_t size) = 0;
    using Stream::read;
    virtual void flush() = 0;
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
};
