# Abhängigkeiten

Ziel: Dieses Projekt muss auch in Jahren noch baubar sein. Deshalb liegt **alles, was nicht zum Board-Paket gehört,
in diesem Ordner**. Es wird keine Bibliothek aus dem Arduino-Bibliotheksverwalter oder von GitHub nachgeladen.
`#include <…>` mit spitzen Klammern bezieht sich nur auf das Board-Paket (siehe unten), alles andere ist mit
Anführungszeichen eingebunden und liegt hier.

## 1. Was NICHT im Ordner liegt: das Board-Paket

| Was | Version |
|---|---|
| Board-Paket **arduino-pico** (Earle Philhower), liefert `Arduino.h`, `Wire.h`, `EEPROM.h`, `Serial2` | **_nicht eingetragen – siehe unten_** |
| Board (FQBN) | `rp2040:rp2040:waveshare_rp2040_eth` (bitte beim ersten Bau bestätigen: `arduino-cli board listall rp2040`) |
| Arduino-IDE oder arduino-cli | die, mit der gebaut wurde: steht nach jedem Bau in `BUILD-INFO.txt` |

**Die Kernversion ist hier noch nicht eingetragen**, weil sie nur auf dem Rechner mit dem funktionierenden Bau
bekannt ist. Beim nächsten erfolgreichen Bau `tools/build.sh` ausführen: es schreibt die tatsächlich benutzten
Versionen in `BUILD-INFO.txt`; diese Datei einchecken und die Zeile oben anpassen. Installieren einer bestimmten
Version: `arduino-cli core install rp2040:rp2040@<Version>` (in der IDE: Boardverwalter, Version wählen).
Alte Versionen bleiben in den Releases des Board-Pakets verfügbar; zusätzlich die Installationsdatei des Pakets
zusammen mit dem Projekt archivieren, wenn es lange laufen soll.

## 2. Was im Ordner liegt

Alle Dateien dieser Tabelle sind per SHA-256 in `DEPENDENCIES.sha256` festgehalten. `tools/build.sh --check-deps`
prüft, ob sich still etwas geändert hat.

| Datei(en) | Zweck | Herkunft / Version |
|---|---|---|
| `src/AMLink/` | Verbindung zum AusbildungsManager (Protokoll v2) | eigene Bibliothek, Stand steht in `src/AMLink/VERSION` (Version + Commit). **Nicht von Hand ändern**, sondern im Repo `ausbildungsmanager-amlink` mit `tools/vendor.sh` aktualisieren |
| `Wiegand.cpp`, `Wiegand.h` | Wiegand-Leser | Herkunft im Code nicht vermerkt (Quelle bitte nachtragen) |
| `CH9120.cpp`, `CH9120.h`, `CH9120.cfg` | Einrichtung der Netzwerkbrücke (Waveshare-Beispielcode, angepasst) | Waveshare; `CH9120.cpp` enthält **Ziel-IP und -Port** und ist daher nicht in der Prüfsummenliste |
| `LcdText.h` / `*.ino` | eigener Code | dieses Repository |

Beim Hinzufügen einer Bibliothek: Datei(en) in den Ordner kopieren, mit Anführungszeichen einbinden, hier eintragen
(Quelle, Version, Lizenz) und `DEPENDENCIES.sha256` neu erzeugen:

```bash
sha256sum Wiegand.cpp Wiegand.h ... > DEPENDENCIES.sha256 && find src/AMLink -type f | sort | xargs sha256sum >> DEPENDENCIES.sha256
```

## 3. Bauen und festhalten

```bash
tools/build.sh                # prüft die Prüfsummen, baut, schreibt BUILD-INFO.txt
tools/build.sh --check-deps   # nur prüfen
```

`BUILD-INFO.txt` (nach jedem Release einchecken) enthält Commit, arduino-cli-Version, alle Board-Paket-Versionen und
die Prüfsumme der gebauten Firmware. Damit lässt sich später nachvollziehen, womit eine laufende Firmware gebaut wurde.

## 4. PC-Test ohne Board

`test/host` baut den Sketch als PC-Programm; es nutzt nur Dateien aus diesem Ordner (`src/AMLink`,
`test/host/arduino-stubs`) und einen C++-Compiler. Das ersetzt den Bau fürs Board **nicht**.
