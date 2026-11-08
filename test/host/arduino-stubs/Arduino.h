#pragma once
// Just enough of the Arduino API to compile the AMLink adapters and example sketches on a PC (tests only).
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

extern int g_pinLevel[64];
extern uint32_t g_millis;
inline void pinMode(int, int) {}
inline int digitalRead(int pin) { return g_pinLevel[pin]; }
inline void digitalWrite(int pin, int level) { g_pinLevel[pin] = level; }
inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t) {}

// Board pin names used by the sketches (values are arbitrary here)
enum { D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15, D16, D17, D18, D19, D20, D21, D22, D26 = 26, D27, D28 };
extern bool BOOTSEL;   // arduino-pico: true while the BOOTSEL button is pressed
