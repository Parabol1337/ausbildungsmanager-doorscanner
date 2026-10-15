#include "CH9120.h"
#include "Wiegand.h"
#include <ArduinoJson.h>

// Software
#define Pin_Timeout 3000

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
JsonDocument Json_Rx;
JsonDocument Json_Tx;

String Pin_Cache = "";
unsigned long Pin_LastPress = 0;
bool Bell_Bounce = LOW;
byte Last_DoorState = 2;
String TCP_Out;
String TCP_In;
bool isRegistered = false;

void setup() {
    CH9120_init();
    WG_Scanner.begin(WG_D0, WG_D1);
    Debug_Serial.begin(9600);

    pinMode(Door_Out, OUTPUT);
    pinMode(Bell_In, INPUT_PULLUP);
    pinMode(Door_In, INPUT); // Türkontakt

    Debug_Serial.println("Client gestartet");
}

void loop() {
    // Verbindung prüfen
    if (digitalRead(TCP_Disconnected)) {
        Debug_Serial.println("Warten auf Verbindung zum Server");
        isRegistered = false; // ✅ Registrierung verlorgen
        while (digitalRead(TCP_Disconnected)) { delay(100); }
        Debug_Serial.println("Verbindung zum Server hergestellt");
    }

    // PIN Timeout
    if (millis() - Pin_LastPress >= Pin_Timeout) {
        Pin_LastPress = millis();
        Pin_Cache = "";
    }

    // TCP Empfang
    while (TCP_Serial.available()) {
        TCP_In = TCP_Serial.readString();
        Debug_Serial.println(TCP_In);

        DeserializationError err = deserializeJson(Json_Rx, TCP_In);
        if (err != DeserializationError::Ok) {
            Json_Tx.clear();
            Json_Tx["error"] = "dataformat";
            serializeJson(Json_Tx, TCP_Out);
            TCP_Serial.println(TCP_Out);
            return;
        }

        String cmd = Json_Rx["cmd"].as<String>();

        if (cmd == "welcome") {
            Json_Tx.clear();
            Json_Tx["cmd"] = "welcome";
            Json_Tx["token"] = rp2040.getChipID();
            Json_Tx["ch"][0] = "door";
            serializeJson(Json_Tx, TCP_Out);
            TCP_Serial.println(TCP_Out);
            Debug_Serial.println(TCP_Out);
        }

         if (cmd == "registered") {
            isRegistered = true; // ✅ Registrierung abgeschlossen
        }

        if (cmd == "open" && isRegistered && Json_Rx["ch"].as<int>() == 1) {
            digitalWrite(Door_Out, HIGH);
            delay(50);
            digitalWrite(Door_Out, LOW);
            Json_Tx.clear();
            Json_Tx["cmd"] = "opened";
            Json_Tx["ch"] = 1;
            serializeJson(Json_Tx, TCP_Out);
            TCP_Serial.println(TCP_Out);
        }
    }

    // Wiegand Daten
    if (isRegistered && WG_Scanner.available()) {
        long code = WG_Scanner.getCode();
        int type = WG_Scanner.getWiegandType();
        Debug_Serial.printf("Wiegand HEX = %lX, Type W%d\n", code, type);

        if (code != 0x27C2) {
            if (type == 34) {
                char rfid[10];
                sprintf(rfid, "%08lx", code);
                Json_Tx.clear();
                Json_Tx["cmd"] = "access";
                Json_Tx["rfid"] = rfid;
                Json_Tx["ch"] = 1;
                serializeJson(Json_Tx, TCP_Out);
                TCP_Serial.println(TCP_Out);
            }

            if (type == 4) {
                Pin_LastPress = millis();
                if (code >= 0 && code <= 9) {
                    Pin_Cache += String(code);  // Ziffer anhängen
                }
                if (code == 27) { // ESC oder Cancel
                    Pin_Cache = "";
                }
                if (code == 13) {
                    Json_Tx.clear();
                    Json_Tx["cmd"] = "access";
                    Json_Tx["pin"] = Pin_Cache;
                    Json_Tx["ch"] = 1;
                    serializeJson(Json_Tx, TCP_Out);
                    TCP_Serial.println(TCP_Out);
                    Pin_Cache = "";
                }
            }
        }
    }

    // Klingel-Taste
    if (isRegistered && !digitalRead(Bell_In) != Bell_Bounce) {
        Bell_Bounce = !digitalRead(Bell_In);
        if (!digitalRead(Bell_In)) {
            Json_Tx.clear();
            Json_Tx["cmd"] = "bell";
            Json_Tx["ch"] = 1;
            serializeJson(Json_Tx, TCP_Out);
            TCP_Serial.println(TCP_Out);
        }
    }

    // Zustand (Tür offen/geschlossen) regelmäßig senden
    if (isRegistered) {
      bool DoorState = digitalRead(Door_In);
      if (DoorState != Last_DoorState) {
          Json_Tx.clear();
          Json_Tx["cmd"] = "status";
          Json_Tx["ch"] = 1;
          Json_Tx["state"] = DoorState ? "open" : "closed";
          serializeJson(Json_Tx, TCP_Out);
          TCP_Serial.println(TCP_Out);
          Last_DoorState = DoorState;
      }
    }
    delay(50); // Entlastung
}
