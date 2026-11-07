/*
 * Tuerscanner - Waveshare RP2040-ETH (CH9120 network bridge) with a Wiegand reader.
 *
 * Uses the AMLink library for the AusbildungsManager hardware interface (protocol v2). The device only reports what
 * happens (card, PIN, bell, door contact) and executes what the server tells it (open the door). Who may enter is
 * decided in the AusbildungsManager - there is no access logic in this firmware any more.
 *
 * Needs: arduino-pico core, library AMLink (https://github.com/Parabol1337/ausbildungsmanager-amlink).
 * Setup: see README.md (target address and port of the server in CH9120.cpp, pairing).
 */
#include <AMLink.h>

#include "CH9120.h"
#include "Wiegand.h"

// Software
#define Pin_Timeout 3000        // a PIN entry is dropped after this many ms without a key
#define Default_Pulse_Ms 50     // door opener pulse if the server does not ask for a specific duration

// Hardware
#define WG_D0 D7
#define WG_D1 D8
#define Bell_In D5
#define Door_Out D2
#define Door_In D1
#define TCP_Disconnected D17

#define TCP_Serial Serial2
#define Debug_Serial Serial

WIEGAND WG_Scanner;

// The CH9120 holds the TCP connection; AMLink talks to it over the serial port. The device secret lives in the
// flash-backed EEPROM emulation of the RP2040.
amlink::SerialBridgeTransport transport(TCP_Serial, TCP_Disconnected);
amlink::EepromStorage storage;
AMLink amLink(transport, storage);
amlink::PulseOutput opener(Door_Out);

String Pin_Cache = "";
unsigned long Pin_LastPress = 0;
bool Bell_WasPressed = false;
int Door_LastState = -1;

// The server asks to open the door: energise the opener for the requested time without blocking the loop.
int onDoor(void*, amlink::Call& call) {
  if (call.is("unlock")) {
    opener.start(call.intArg("pulse_ms", Default_Pulse_Ms));
    amLink.setState("door", "unlocked");
    return amlink::AM_OK;
  }
  return amlink::AM_UNSUPPORTED;
}

// Pairing button: BOOTSEL, or the doorbell button (easier to reach inside a door station).
bool pairingPressed(void*) { return BOOTSEL || digitalRead(Bell_In) == LOW; }

void onStatus(void*, amlink::Status status) {
  switch (status) {
    case amlink::StatusOnline: Debug_Serial.println("Server: verbunden"); break;
    case amlink::StatusConnecting: Debug_Serial.println("Server: Anmeldung"); break;
    case amlink::StatusOffline: Debug_Serial.println("Server: nicht erreichbar"); break;
    case amlink::StatusPairing:
      Debug_Serial.print("Warten auf Freigabe, Kopplungscode: ");
      Debug_Serial.println(amLink.pairingCode());
      break;
  }
}

void setup() {
  CH9120_init();  // sets up the bridge; it connects to the server by itself
  WG_Scanner.begin(WG_D0, WG_D1);
  Debug_Serial.begin(9600);

  pinMode(Bell_In, INPUT_PULLUP);
  pinMode(Door_In, INPUT);  // door contact
  opener.begin();

  if (!amLink.begin(amlink::Identity("rp2040-eth-door", "2.0.0"))) {
    Debug_Serial.println("AMLink konnte nicht gestartet werden (Geheimnis nicht speicherbar?)");
  }
  amLink.setPairingButton(pairingPressed);
  amLink.onStatus(onStatus);

  amLink.entity("reader", "reader")->label("RFID-Leser").feature("rfid").feature("pin");
  amLink.entity("door", "lock")->label("Türöffner").feature("unlock").attr("max_pulse_ms", 10000L).onAction(onDoor);
  amLink.entity("door_contact", "binary_sensor")->label("Türkontakt").cls("door");
  amLink.entity("bell", "button")->label("Klingel");
  amLink.setState("door", "locked");

  Debug_Serial.println("Client gestartet");
}

void loop() {
  amLink.loop();  // connection, login, signatures, ping - never blocks

  // End of the opener pulse
  if (opener.update()) amLink.setState("door", "locked");

  // Door contact: "on" = door open. Reported on change only.
  int door = digitalRead(Door_In) ? 1 : 0;
  if (door != Door_LastState) {
    Door_LastState = door;
    amLink.setState("door_contact", door ? "on" : "off");
  }

  // Doorbell: one event per press
  bool bell = digitalRead(Bell_In) == LOW;
  if (bell && !Bell_WasPressed) amLink.event("bell", "press");
  Bell_WasPressed = bell;

  // A PIN entry is dropped after a pause
  if (Pin_Cache.length() && millis() - Pin_LastPress >= Pin_Timeout) Pin_Cache = "";

  // Wiegand reader: 34 bit = card, 4 bit = key on the keypad
  if (WG_Scanner.available()) {
    unsigned long code = WG_Scanner.getCode();
    int type = WG_Scanner.getWiegandType();
    Debug_Serial.printf("Wiegand HEX = %lX, Type W%d\n", code, type);

    if (code != 0x27C2) {
      if (type == 34) {
        char rfid[10];
        snprintf(rfid, sizeof(rfid), "%08lx", code);
        amlink::EventData data;
        data.str("method", "rfid").str("value", rfid);
        amLink.event("reader", "scan", data);
      }

      if (type == 4) {
        Pin_LastPress = millis();
        if (code <= 9) {
          Pin_Cache += String(code);  // append digit
        } else if (code == 27) {      // ESC / cancel
          Pin_Cache = "";
        } else if (code == 13) {      // Enter
          amlink::EventData data;
          data.str("method", "pin").str("value", Pin_Cache.c_str());
          amLink.event("reader", "scan", data);
          Pin_Cache = "";
        }
      }
    }
  }
}
