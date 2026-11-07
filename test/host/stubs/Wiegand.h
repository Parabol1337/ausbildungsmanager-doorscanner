#pragma once
// Stand-in for the interrupt driven Wiegand reader: codes are fed in by the host runner instead of the data lines.
// Defines the include guard of the real Wiegand.h so that the real header is skipped.
#ifndef _WIEGAND_H
#define _WIEGAND_H

#include "Arduino.h"

class WIEGAND {
public:
    void begin() {}
    void begin(int, int) {}
    bool available() { return count_ > 0; }
    unsigned long getCode() { return codes_[head_]; }
    int getWiegandType() { int t = types_[head_]; head_ = (head_ + 1) % 16; count_--; return t; }
    // host runner
    void push(unsigned long code, int type) { if (count_ < 16) { int at = (head_ + count_) % 16; codes_[at] = code; types_[at] = type; count_++; } }
private:
    unsigned long codes_[16] = {};
    int types_[16] = {};
    int head_ = 0, count_ = 0;
};

#endif
