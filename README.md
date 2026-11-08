# Türscanner (RP2040-ETH)

Firmware für den Türscanner der Ausbildungswerkstatt: Waveshare **RP2040-ETH** mit **CH9120**-Netzwerkbaustein,
Wiegand-Leser (Karte und PIN), Klingel, Türkontakt und Türöffner. Die Firmware spricht mit dem AusbildungsManager über
die Hardware-Schnittstelle ([Protokoll v2](https://github.com/Parabol1337/ausbildungsmanager-next/blob/main/docs/hardware/README.md))
mit der Bibliothek **AMLink**.

Das Gerät entscheidet nichts: Es meldet, was passiert (Karte gelesen, PIN eingegeben, Klingel, Tür auf/zu), und führt
aus, was der Server verlangt (Tür öffnen). Wer wann eintreten darf, steht im AusbildungsManager.

## Aufbau

| Entität | Domäne | Bedeutung |
|---|---|---|
| `reader` | reader | Wiegand-Leser, Ereignis `scan` mit `method` = `rfid` oder `pin` |
| `door` | lock | Türöffner, Aktion `unlock` (`pulse_ms`, Standard 50 ms, höchstens 10 s) |
| `door_contact` | binary_sensor | Türkontakt, `on` = Tür offen |
| `bell` | button | Klingel, Ereignis `press` |

Pinbelegung und Verhalten (34-Bit-Karte, PIN mit Enter, 3 s Eingabe-Timeout) sind wie in der bisherigen Firmware.
Die bisherige Zeilen-/`cmd`-Logik und die Abhängigkeit von ArduinoJson entfallen.

## Einrichtung

1. **Arduino IDE** mit dem Kern *Raspberry Pi Pico/RP2040* (arduino-pico) für das Board Waveshare RP2040-ETH.
2. Es muss **nichts weiter installiert werden**: AMLink und alle anderen Bibliotheken liegen im Ordner
   (`src/AMLink`, siehe [DEPENDENCIES.md](DEPENDENCIES.md); dort steht auch die Version des Board-Pakets).
3. In `CH9120.cpp` Server und Port eintragen: `CH9120_TARGET_IP` (Adresse des AusbildungsManagers) und
   `CH9120_TARGET_PORT = 8010` (Hardware-Gateway; bisher stand dort 8000). Der Chip verbindet sich damit von selbst.
4. Sketch hochladen. Auf der seriellen Konsole (9600 Baud) erscheint der Zustand und beim ersten Start der **Kopplungscode**.
5. **Freigabe** im AusbildungsManager unter *Administration → Hardware*: Das Gerät erscheint als „Wartet auf Freigabe“.
   Entweder den angezeigten Code vergleichen und „Code stimmt – freigeben“, oder „Freigabe am Gerät starten“ und innerhalb
   von zwei Minuten die **Klingel** (oder BOOTSEL) drücken. Danach Standort und Raum zuordnen.

Das Geheimnis des Geräts wird beim ersten Start erzeugt und im Flash gespeichert. Ein Neuaufspielen der Firmware behält
es. Soll das Gerät als neues Gerät angemeldet werden, im AusbildungsManager löschen und das Geheimnis zurücksetzen
(`amLink.factoryReset()`).

## Tests auf dem Rechner

Der **unveränderte Sketch** lässt sich als Programm für den Rechner bauen. Der CH9120, der Wiegand-Leser, die Klingel
und der Türkontakt werden darum herum simuliert; die Verbindung geht über einen echten Socket zum echten Gateway. Der
Test dafür liegt im Server-Repository (`tests/Feature/Hardware/DoorScannerSketchTest.php`):

```bash
# im Server-Repository (ausbildungsmanager-next), beide Repositories als Nachbarordner
php artisan test --filter=DoorScannerSketchTest
```

Er prüft Einrichtung, Kopplung über die Klingel, Karte und PIN bis zur Türöffnung (mit gemessener Impulsdauer), Klingel,
Türkontakt und das erneute Anmelden nach einem Neustart des Servers. Der Pfad lässt sich über `DOORSCANNER_PATH` setzen. Gebaut wird mit `make -C test/host` (nur ein C++-Compiler nötig).

**Nicht geprüft** ist der Bau und Lauf auf dem echten Board mit dem arduino-pico-Kern: Der Test ersetzt Arduino-Klassen,
Treiber und CH9120 durch Nachbauten. Zum Beispiel muss `BOOTSEL` im verwendeten Kern verfügbar sein.
