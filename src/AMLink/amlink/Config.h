#pragma once
// AMLink compile-time configuration. Define any of these BEFORE including AMLink.h to override.

#include <stddef.h>
#include <stdint.h>

#define AMLINK_VERSION "1.0.0"
#define AMLINK_PROTOCOL 2

// Maximum number of entities one device can announce.
#ifndef AMLINK_MAX_ENTITIES
#define AMLINK_MAX_ENTITIES 40
#endif

// Size of the receive buffer = the longest line the SERVER may send to this device. It is announced to the
// server (`max_line`), which then never sends longer lines. 1024 is plenty for door/lock devices; raise it for
// displays that show many text lines.
#ifndef AMLINK_LINE_SIZE
#define AMLINK_LINE_SIZE 1024
#endif

// Size of the send buffer = the longest line this device sends. Larger buffers need fewer lines to announce
// many entities. The login message must fit, hence the minimum.
#ifndef AMLINK_SEND_SIZE
#define AMLINK_SEND_SIZE 1024
#endif

#if AMLINK_LINE_SIZE < 256
#error "AMLINK_LINE_SIZE must be at least 256"
#endif
#if AMLINK_SEND_SIZE < 512
#error "AMLINK_SEND_SIZE must be at least 512"
#endif

// Longest model / firmware text accepted by Link::begin() so that the login message always fits the send buffer.
#define AMLINK_MAX_MODEL 48
#define AMLINK_MAX_FIRMWARE 24

// Limits of the descriptive data of one entity (pointers are kept, the strings must outlive the link).
#ifndef AMLINK_MAX_FEATURES
#define AMLINK_MAX_FEATURES 6
#endif
#ifndef AMLINK_MAX_ATTRS
#define AMLINK_MAX_ATTRS 5
#endif
