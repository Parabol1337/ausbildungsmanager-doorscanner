#pragma once
/*
 * AMLink - connects a device to the AusbildungsManager hardware interface (protocol v2).
 *
 *   #include <AMLink.h>
 *
 * See README.md and the examples. The same library runs on ESP32 (Wi-Fi or Ethernet), RP2040 behind a CH9120
 * bridge or with a W5500, and on a PC for tests.
 */
#include "amlink/Config.h"
#include "amlink/Entity.h"
#include "amlink/Env.h"
#include "amlink/Link.h"
#include "amlink/Storage.h"
#include "amlink/Transport.h"

#if defined(ARDUINO)
#include "amlink/arduino/ClientTransport.h"
#include "amlink/arduino/EepromStorage.h"
#include "amlink/arduino/PreferencesStorage.h"
#include "amlink/arduino/PulseOutput.h"
#include "amlink/arduino/SerialBridgeTransport.h"
#endif

typedef amlink::Link AMLink;
