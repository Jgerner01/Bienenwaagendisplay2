# Bedienungsanleitung – Bienenwaagendisplay 2

---

## Inhaltsverzeichnis

1. [Übersicht](#1-übersicht)
2. [Display-Anzeige und Anzeigemodi](#2-display-anzeige-und-anzeigemodi)
3. [Taster – Ertragsmessung und Schnellmessung](#3-taster--ertragsmessung-und-schnellmessung)
4. [Erstinbetriebnahme – WiFi einrichten](#4-erstinbetriebnahme--wifi-einrichten)
5. [WiFi Auto-Reconnect und Reboot](#5-wifi-auto-reconnect-und-reboot)
6. [Webinterface aufrufen](#6-webinterface-aufrufen)
7. [Bereich: Waage](#7-bereich-waage)
8. [Bereich: Parameter](#8-bereich-parameter)
   - [8.1 Tara setzen](#81-tara-setzen)
   - [8.2 Kalibrieren](#82-kalibrieren)
   - [8.3 Gain-Faktor](#83-gain-faktor)
   - [8.4 Poly2 T-Korrektur (Stufe 1)](#84-poly2-t-korrektur-stufe-1)
   - [8.5 PT2 Korrektur (Stufe 2)](#85-pt2-korrektur-stufe-2)
9. [MQTT konfigurieren](#9-mqtt-konfigurieren)
10. [Firmware-Update (OTA)](#10-firmware-update-ota)
11. [Fehlerbehebung](#11-fehlerbehebung)

---

## 1. Übersicht

Die Bienenwaage erfasst kontinuierlich das Gewicht des Bienenstocks sowie die Temperatur. Die Messwerte werden auf einem LCD-Display angezeigt und optional per MQTT an ein Smart-Home-System (z. B. Home Assistant) übertragen.

**Funktionen auf einen Blick:**

| Funktion | Beschreibung |
|---|---|
| Gewichtsmessung | HX711-Wägeverstärker, ±1 g Auflösung |
| Temperaturmessung | DS18B20, ±0,5 °C |
| Poly2 T-Korrektur | Kompensation temperaturbedingter Gewichtsdrift (Stufe 1) |
| PT2 Korrektur | PT2-gefilterter Temperatureinfluss (Stufe 2) |
| Ertragsmessung | Langzeit-Referenz für Stockertrag, per Taster oder Web |
| Schnellmessung | Kurzzeit-Messung für aufgelegte Gegenstände, per Taster |
| Display | LCD 16×2 mit drei Anzeigemodi |
| Webinterface | Konfiguration und Kalibrierung im Browser |
| MQTT | Datenübertragung an Home Assistant oder andere Broker |
| OTA | Firmware-Update über WLAN |
| Auto-Reconnect | Automatische Wiederverbindung bei WLAN-Ausfall |

---

## 2. Display-Anzeige und Anzeigemodi

Das Display hat drei Anzeigemodi, die durch den Taster (D8) umgeschaltet werden.

### Normalmodus

```
T:  18.8 C
   26.547 kg
```

Zeile 1: Temperatur | Zeile 2: korrigiertes Gewicht (oder Ertragswert, wenn gesetzt)

```
T:  18.8 C
E: +0.235 kg
```

Wenn die Ertragsmessung aktiv ist, zeigt Zeile 2 den Ertragswert (aktuelles Gewicht minus Ertragsbasis, mit Vorzeichen).

### Schnellmessung-Modus

```
Schnellmessung
  +0.567 kg
```

Wird 2× pro Sekunde aktualisiert. Zeigt das Gewicht relativ zur Schnell-Tara.

### Bestätigungs-Modus (Ertragsmessung tarieren)

```
Ertrag tarieren?
Taste->OK (5s)
```

Erscheint nach langem Tastendruck (≥ 5 s). Innerhalb von 5 s kurz drücken zum Bestätigen.

**Beim Einschalten** wird kurz die IP-Adresse angezeigt:

```
Bienenwaage
192.168.1.42
```

Im AP-Mode:

```
Bienenwaage AP
192.168.4.1
```

---

## 3. Taster – Ertragsmessung und Schnellmessung

Ein Taster ist an **D8 (GPIO15)** angeschlossen. Er benötigt einen externen **10 kΩ Pull-Down-Widerstand** nach GND. Beim Drücken wird D8 mit 3,3 V verbunden.

### Schnellmessung

| Aktion | Ergebnis |
|---|---|
| Kurzer Druck (< 5 s) | Schnell-Tara setzen, Schnellmessung starten |
| Nochmals kurzer Druck | Schnellmessung beenden |

**Verwendung:** Gegenstand auf den Stock legen → kurz drücken → Display zeigt das Gewicht des Gegenstands relativ zur Schnell-Tara.

### Ertragsmessung

| Aktion | Ergebnis |
|---|---|
| ≥ 5 s halten, dann loslassen | Bestätigungsfenster öffnet sich (5 s) |
| Kurzer Druck im Fenster | Ertragsbasis auf aktuelles Gewicht setzen |
| Kein Druck im Fenster | Abbruch, keine Änderung |

**Verwendung:** Zum Saisonstart oder nach der Einwinterung: langer Tastendruck, dann bestätigen. Ab sofort zeigt Zeile 2 den Gewichtsgewinn seit diesem Zeitpunkt.

> **Die Ertragsbasis wird dauerhaft gespeichert** und bleibt auch nach einem Stromausfall erhalten.

> Die Ertragsmessung kann auch im Webinterface unter **Waage** gesetzt und zurückgesetzt werden.

---

## 4. Erstinbetriebnahme – WiFi einrichten

Beim ersten Start öffnet das Gerät einen eigenen WLAN-Hotspot:

| Einstellung | Wert |
|---|---|
| SSID | `Bienenwaage` |
| Passwort | `12345678` |
| IP-Adresse | `192.168.4.1` |

**Schritte:**

1. Mit Smartphone oder PC mit dem WLAN `Bienenwaage` verbinden.
2. Browser öffnen → `192.168.4.1`.
3. **WiFi** antippen → Heimnetzwerk aus der Liste wählen → Passwort eingeben → **Verbinden**.
4. Die neue IP-Adresse erscheint auf dem Display.
5. Im Heimnetzwerk die angezeigte IP im Browser öffnen.

> **Tipp:** Die aktuelle SSID wird im WiFi-Formular vorausgefüllt. Das Passwortfeld kann leer gelassen werden – das gespeicherte Passwort bleibt dann unverändert.

> Der Konfigurationspunkt (192.168.4.1) bleibt 2 Minuten erreichbar, während das Gerät bereits im Heimnetzwerk ist.

---

## 5. WiFi Auto-Reconnect und Reboot

Das Gerät versucht bei Verbindungsabbruch automatisch, die Verbindung wiederherzustellen:

| Phase | Beschreibung |
|---|---|
| Verbindungsabbruch | Sofort erster Reconnect-Versuch |
| Reconnect-Versuche | Alle 30 Sekunden, bis zu 10× |
| Nach 10 Fehlversuchen (5 min) | Wechsel in AP-Mode (`192.168.4.1`) |
| 10 min im AP-Mode ohne Verbindung | Automatischer Neustart |

Nach dem Neustart verbindet sich das Gerät erneut mit dem gespeicherten WLAN. Falls dieses erreichbar ist, läuft der Betrieb normal weiter. Andernfalls wiederholt sich der Zyklus.

---

## 6. Webinterface aufrufen

```
http://<IP-Adresse>/
```

Die IP-Adresse steht beim Start kurz auf dem Display oder im Router unter den verbundenen Geräten (Hostname: `bienenwaage`).

**Navigation:**

| Menüpunkt | Funktion |
|---|---|
| Waage | Messwerte (3 Gewichtswerte), Ertragsmessung setzen/zurücksetzen |
| Parameter | Tara, Kalibrierung, Gain, Poly2 T-Korrektur, PT2 Korrektur |
| MQTT | MQTT-Broker konfigurieren |
| WiFi | Netzwerk wechseln |
| OTA | Firmware-Update |

---

## 7. Bereich: Waage

Unter **Waage** werden drei Gewichtswerte angezeigt:

| Wert | Beschreibung |
|---|---|
| Gewicht (roh) | Unkompensierter Messwert |
| Gewicht (T-korr.) | Nach Poly2 T-Korrektur (Stufe 1), `*` wenn aktiv |
| Gewicht (T+PT2-korr.) | Nach Poly2 + PT2-Korrektur (Stufe 1+2), `**` wenn aktiv |

Zusätzlich:

- **Ertragswert**: Gewicht relativ zur gesetzten Ertragsbasis
- **Ertragsbasis jetzt setzen**: Setzt das aktuelle vollständig korrigierte Gewicht als Referenz
- **Ertragsmessung zurücksetzen**: Deaktiviert die Ertragsmessung

---

## 8. Bereich: Parameter

Alle Messparameter sind direkt auf einer Seite editierbar: **Webinterface → Parameter**.

### 8.1 Tara setzen

Kompensiert das Eigengewicht der Beutenteile.

**Wann setzen?** Nach dem Aufbau, nach dem Hinzufügen von Beutenteilen oder wenn das Gewicht ohne Grund von Null abweicht.

1. Alle Beutenteile auflegen (ohne Bienen/Honig, falls gewünscht).
2. **Parameter → Tara setzen** klicken.

> Der Tara-Wert wird dauerhaft gespeichert.

### 8.2 Kalibrieren

**Voraussetzung:** Tara gesetzt, bekanntes Referenzgewicht (z. B. 5–20 kg).

1. Referenzgewicht auflegen.
2. **Parameter → Bekanntes Gewicht** eintragen (z. B. `5.250`).
3. **Kalibrieren** klicken.

> Möglichst ein Gewicht nahe dem typischen Stockgewicht wählen.

### 8.3 Gain-Faktor

| Gain | Kanal | Empfehlung |
|---|---|---|
| 128 | A | Standard – beste Auflösung (voreingestellt) |
| 64 | A | Bei Übersteuerung mit Gain 128 |
| 32 | B | Zweiter Eingang (sofern verdrahtet) |

**Parameter → Gain-Faktor auswählen → Speichern.**

> Nach einer Gain-Änderung die Kalibrierung wiederholen.

### 8.4 Poly2 T-Korrektur (Stufe 1)

Korrigiert die temperaturbedingte Gewichtsdrift mit einem Polynom 2. Ordnung:

```
Korrektur(T) = a·T² + b·T + c
Gewicht (T-korr.) = Rohgewicht − Korrektur(T)
```

**Koeffizienten manuell eintragen:** Parameter → Poly2 T-Korrektur → a, b, c → Speichern.

**Koeffizienten aus Messdaten berechnen (empfohlen):**

1. Messdaten über ≥ 10 °C Temperaturbereich aufzeichnen (konstantes Gewicht!).
2. CSV-Dateien aus Home Assistant exportieren (Gewicht + Temperatur).
3. **Parameter → CSV-Fit Assistent →** Button klickt auf `/tempcal`.
4. Beide CSV-Dateien hochladen → **Berechnen**.
5. R² prüfen (> 0,8 anstreben) → **Speichern**.

### 8.5 PT2 Korrektur (Stufe 2)

Die PT2-Korrektur berücksichtigt thermisch träge Effekte: Die Temperatur wird zunächst durch einen PT2-Tiefpassfilter geleitet, dann wird erneut ein Poly2 auf die gefilterte Temperatur angewendet:

```
T_pt2  = PT2-Filter(T, T₂, D)
Korrektur(T_pt2) = a·T_pt2² + b·T_pt2 + c
Gewicht (T+PT2-korr.) = Gewicht (T-korr.) − Korrektur(T_pt2)
```

**Parameter:**

| Parameter | Beschreibung | Typischer Wert |
|---|---|---|
| T₂ (Minuten) | Zeitkonstante des PT2-Filters | 120–480 min |
| D (Dämpfung) | 0,5 = schwingungsfähig, 0,7 = kritisch, 1,0 = überdämpft | 0,7 |
| a, b, c | Poly2-Koeffizienten für die gefilterte Temperatur | aus CSV-Fit |

**Koeffizienten berechnen:**

1. **Parameter → CSV-Fit Assistent (PT2) →** öffnet `/pt2cal`.
2. T₂ und D einstellen.
3. CSV-Dateien hochladen → **Berechnen**.
4. Der Browser filtert die Temperatur mit den PT2-Parametern und fittet Poly2.
5. R² prüfen, T₂ ggf. anpassen → **Speichern**.

> Stufe 1 und Stufe 2 können unabhängig aktiviert werden. Für die meisten Anwendungen reicht Stufe 1 (Poly2 direkt).

---

## 9. MQTT konfigurieren

**Voraussetzung:** MQTT-Broker im Netzwerk (z. B. Mosquitto).

**Webinterface → MQTT:**

| Feld | Beispiel | Beschreibung |
|---|---|---|
| Broker | `192.168.1.10` | IP-Adresse des Brokers |
| Port | `1883` | Standard-Port |
| Benutzername | (optional) | Authentifizierung |
| Passwort | (optional) | |
| Client-ID | `bienenwaage_01` | Eindeutige Geräte-ID |
| Topic-Prefix | `bienenwaage/01` | Präfix für alle Topics |
| Publish-Intervall | `60` | Sekunden |
| MQTT aktiviert | Ja | |
| HA Auto-Discovery | Ja | Sensoren automatisch in HA anlegen |

**Übertragene Messwerte:**

| Topic | Inhalt |
|---|---|
| `.../sensors/weight` | Rohgewicht in kg |
| `.../sensors/weight_t_corrected` | Gewicht nach Poly2 T-Korrektur (Stufe 1) |
| `.../sensors/weight_corrected` | Gewicht nach T+PT2-Korrektur (Stufe 1+2) |
| `.../sensors/ertragsgewicht` | Ertragswert in kg (nur wenn Ertragsmessung aktiv) |
| `.../sensors/temperature` | Temperatur in °C |
| `.../sensors/trimmedmean` | Getrimmter Mittelwert in kg |
| `.../sensors/spread` | Standardabweichung in kg |
| `.../sensors/raw` | HX711 Rohwert |
| `.../status` | `online` / `offline` |

---

## 10. Firmware-Update (OTA)

1. Firmware bauen:
   ```
   pio run
   ```
   Datei: `.pio/build/nodemcuv2/firmware.bin`

2. **Webinterface → OTA → Datei auswählen → Hochladen.**
3. Nach erfolgreichem Update startet das Gerät automatisch neu.

> Das Gerät muss während des Updates mit Strom versorgt bleiben.

---

## 11. Fehlerbehebung

### Display zeigt nichts an

- Stromversorgung prüfen.
- I²C-Adresse: Standard `0x3F`, alternativ `0x27` (in `src/config.h` anpassen).
- SDA/SCL prüfen: D1 = SDA, D7 = SCL.

### Gewicht instabil oder springt

- HX711-Verkabelung prüfen.
- Gain-Faktor reduzieren (128 → 64).
- Tara erneut setzen.

### Kein WLAN – Gerät nicht erreichbar

- Das Gerät versucht alle 30 s automatisch die Verbindung wiederherzustellen (bis zu 10×).
- Nach 10 Fehlversuchen wechselt es in den AP-Mode (`Bienenwaage`, 192.168.4.1).
- Nach 10 min im AP-Mode ohne Verbindung: **automatischer Neustart**.
- WiFi-Seite: SSID und Passwort erneut eingeben → **Verbinden**.

### Ertragswert nach Neustart weg

- Die Ertragsbasis wird in `/ertrag.json` gespeichert und übersteht Neustarts.
- Falls der Wert trotzdem fehlt: LittleFS-Speicher prüfen (vollständige Neukonfiguration via `uploadfs`).

### Temperaturkorrektur: R² zu niedrig (< 0,5)

- Zu wenig Temperaturvariation: längeren Zeitraum wählen (≥ 10 °C Spanne).
- Gewicht hat sich während der Aufzeichnung geändert: neuen Datensatz aufzeichnen.
- Für PT2: T₂ anpassen und Berechnung wiederholen.

### MQTT-Verbindung schlägt fehl

- Broker-IP und Port prüfen.
- Broker und Gerät im gleichen Netzwerk?
- Client-ID muss eindeutig sein.

### Taster reagiert nicht

- Externer 10 kΩ Pull-Down-Widerstand zwischen D8 (GPIO15) und GND erforderlich.
- Taster verbindet D8 mit 3,3 V beim Drücken.
