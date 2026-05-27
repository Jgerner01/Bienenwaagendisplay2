# Bedienungsanleitung – Bienenwaagendisplay 2

---

## Inhaltsverzeichnis

1. [Übersicht](#1-übersicht)
2. [Display-Anzeige](#2-display-anzeige)
3. [Erstinbetriebnahme – WiFi einrichten](#3-erstinbetriebnahme--wifi-einrichten)
4. [Webinterface aufrufen](#4-webinterface-aufrufen)
5. [Tara setzen](#5-tara-setzen)
6. [Kalibrieren](#6-kalibrieren)
7. [Gain-Faktor einstellen](#7-gain-faktor-einstellen)
8. [MQTT konfigurieren](#8-mqtt-konfigurieren)
9. [Temperaturkorrektur](#9-temperaturkorrektur)
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
| Temperaturkorrektur | Automatische Kompensation der temperaturbedingten Gewichtsdrift |
| Display | LCD 16×2: Temperatur + Gewicht |
| Webinterface | Konfiguration und Kalibrierung im Browser |
| MQTT | Datenübertragung an Home Assistant oder andere Broker |
| OTA | Firmware-Update über WLAN |

---

## 2. Display-Anzeige

Das LCD zeigt nach dem Start zwei Zeilen:

```
T:  18.8 C
   26.547 kg
```

| Zeile | Inhalt |
|---|---|
| Zeile 1 | Aktuelle Temperatur in °C |
| Zeile 2 | Aktuelles Gewicht in kg (temperaturkorrigiert, wenn aktiv) |

**Beim Einschalten** wird für ca. 5 Sekunden die IP-Adresse angezeigt:

```
Bienenwaage
192.168.1.42
```

Wenn kein WLAN konfiguriert ist:

```
Bienenwaage AP
192.168.4.1
```

---

## 3. Erstinbetriebnahme – WiFi einrichten

Beim ersten Start öffnet das Gerät einen eigenen WLAN-Hotspot:

| Einstellung | Wert |
|---|---|
| SSID | `Bienenwaage` |
| Passwort | `12345678` |
| IP-Adresse | `192.168.4.1` |

**Schritte:**

1. Mit dem Smartphone oder PC mit dem WLAN `Bienenwaage` verbinden.
2. Browser öffnen und `192.168.4.1` eingeben.
3. Auf **WiFi** (Netzwerk-Symbol) tippen.
4. Das Heimnetzwerk aus der Liste auswählen, Passwort eingeben, **Verbinden** klicken.
5. Das Gerät verbindet sich mit dem Heimnetzwerk. Die neue IP-Adresse erscheint auf dem Display und im Browser.
6. Jetzt im Heimnetzwerk anmelden und die angezeigte IP-Adresse im Browser öffnen.

> **Tipp:** Der Konfigurationspunkt (192.168.4.1) bleibt noch 2 Minuten erreichbar, während das Gerät bereits im Heimnetzwerk ist.

---

## 4. Webinterface aufrufen

Das Webinterface ist über jeden Browser im gleichen Netzwerk erreichbar:

```
http://<IP-Adresse>/
```

Die IP-Adresse steht auf dem Display (kurz nach dem Start) oder im Router unter den verbundenen Geräten (Hostname: `bienenwaage`).

**Navigation:**

| Menüpunkt | Funktion |
|---|---|
| Waage | Detailansicht: Rohwert, Offset, Kalibrierfaktor, Tara |
| Kalibrierung | Gewicht kalibrieren und zurücksetzen |
| Gain | HX711 Verstärkungsfaktor einstellen |
| T-Korrektur | Temperaturkorrektur berechnen und aktivieren |
| MQTT | MQTT-Broker konfigurieren |
| WiFi | Netzwerk wechseln |
| OTA | Firmware-Update |

---

## 5. Tara setzen

Das Tara (Nullpunkt) kompensiert das Eigengewicht der Beutenteile.

**Wann Tara setzen?**
- Beim ersten Aufbau, nachdem alle Beutenteile aufgelegt sind.
- Nach dem Hinzufügen oder Entfernen von Beutenteilen.
- Wenn das angezeigte Gewicht ohne Änderung deutlich von Null abweicht.

**Vorgehen:**

1. Alle Beutenteile auf die Waage legen (nur ohne Bienen und Honig, falls gewünscht).
2. Webinterface öffnen → **Waage** → **Tara setzen**.
3. Das Gerät setzt den aktuellen Rohwert als Nullpunkt. Die Anzeige springt auf 0,000 kg.

> Der Tara-Wert wird permanent gespeichert und bleibt auch nach einem Neustart erhalten.

---

## 6. Kalibrieren

Die Kalibrierung stellt sicher, dass das angezeigte Gewicht dem tatsächlichen Gewicht entspricht.

**Benötigt:** Ein bekanntes Referenzgewicht (z. B. ein Stein, der vorher auf einer geeichten Waage gewogen wurde).

**Vorgehen:**

1. Die Waage muss zunächst tariert sein (Schritt 5).
2. Das Referenzgewicht auf die Waage legen.
3. Webinterface → **Kalibrierung**.
4. Das bekannte Gewicht in kg in das Eingabefeld eintragen (z. B. `5.250`).
5. **Kalibrieren** klicken.
6. Der neue Kalibrierfaktor wird gespeichert und sofort angewendet.

> **Tipp:** Möglichst ein Gewicht verwenden, das dem typischen Stockgewicht nahekommt (5–50 kg), um die Genauigkeit zu maximieren.

**Werkseinstellungen zurücksetzen:**

Webinterface → **Kalibrierung** → **Auf Werkseinstellungen zurücksetzen** setzt Kalibrierfaktor, Offset und Gain auf die Standardwerte zurück.

---

## 7. Gain-Faktor einstellen

Der Gain-Faktor bestimmt den Eingangskanal und die Verstärkung des HX711.

| Gain | Kanal | Empfehlung |
|---|---|---|
| 128 | A | Standard – beste Auflösung (voreingestellt) |
| 64 | A | Wenn der Messbereich bei Gain 128 überschritten wird |
| 32 | B | Zweiter Eingang (sofern verdrahtet) |

Webinterface → **Gain** → Wert auswählen → **Speichern**.

> Bei einer Änderung des Gain-Faktors sollte die Kalibrierung wiederholt werden.

---

## 8. MQTT konfigurieren

MQTT ermöglicht die Übertragung der Messwerte an Home Assistant oder andere Smart-Home-Systeme.

**Voraussetzung:** Ein MQTT-Broker im Netzwerk (z. B. Mosquitto).

**Vorgehen:**

1. Webinterface → **MQTT**.
2. Folgende Felder ausfüllen:

| Feld | Beispiel | Beschreibung |
|---|---|---|
| Broker | `192.168.1.10` | IP-Adresse des MQTT-Brokers |
| Port | `1883` | Standard-MQTT-Port |
| Benutzername | (optional) | Falls der Broker eine Authentifizierung erfordert |
| Passwort | (optional) | |
| Client-ID | `bienenwaage_01` | Eindeutige Kennung des Geräts |
| Topic-Prefix | `bienenwaage/01` | Wird allen Topics vorangestellt |
| Publish-Intervall | `60` | Sekunden zwischen den Veröffentlichungen |
| MQTT aktiviert | Ja | |
| HA Auto-Discovery | Ja | Sensoren werden automatisch in Home Assistant angelegt |

3. **Speichern** klicken, dann das Gerät neu starten.

**Übertragene Messwerte:**

| Topic | Inhalt |
|---|---|
| `.../sensors/weight` | Rohgewicht in kg |
| `.../sensors/weight_corrected` | Temperaturkorrigiertes Gewicht in kg |
| `.../sensors/temperature` | Temperatur in °C |
| `.../sensors/trimmedmean` | Getrimmter Mittelwert in kg |
| `.../sensors/spread` | Standardabweichung in kg |
| `.../status` | `online` / `offline` |

---

## 9. Temperaturkorrektur

Wägezellen verändern ihr Messergebnis mit der Temperatur. Die Temperaturkorrektur gleicht diesen Effekt mit einem Polynom 2. Ordnung aus.

### 9.1 Daten aufzeichnen

Für eine gute Kalibrierung werden Messdaten über einen **Temperaturbereich von mindestens 10 °C** benötigt, z. B. über mehrere Tage oder über den Tagesverlauf.

**Wichtig:** Während der Aufzeichnung darf sich das **reale Gewicht nicht ändern** (z. B. leere Beute ohne Bienen verwenden oder sicherstellen, dass keine Ernte oder Fütterung stattfindet).

### 9.2 CSV-Dateien aus Home Assistant exportieren

1. Home Assistant öffnen → **Verlauf** (History).
2. Den gewünschten Zeitraum und die Entität auswählen:
   - Einmal für das Gewicht (`sensor.bienenwaage_..._gewicht`)
   - Einmal für die Temperatur (`sensor.bienenwaage_..._temperatur`)
3. Jeweils auf das Download-Symbol klicken → CSV-Datei speichern.

### 9.3 Koeffizienten berechnen

1. Webinterface → **T-Korrektur**.
2. **Gewicht-CSV** auswählen.
3. **Temperatur-CSV** auswählen.
4. **Berechnen** klicken.
5. Das Ergebnis wird angezeigt:
   - Koeffizienten a, b, c des Korrekturpolynoms
   - R²-Wert (Güte des Fits – sollte > 0,8 sein)
   - Diagramm mit Messpunkten und Kurve
6. Korrektur aktivieren (Ja/Nein) auswählen.
7. **Speichern** klicken.

### 9.4 Ergebnis prüfen

Nach dem Speichern zeigt das Webinterface unter **Startseite** beide Werte:

- **Gewicht (roh):** unkompensierter Messwert
- **Gewicht (korrigiert) \*:** temperaturkompensierter Wert (grün hervorgehoben, wenn aktiv)

Das korrigierte Gewicht wird auch auf MQTT unter `.../sensors/weight_corrected` übertragen.

> Die Korrektur kann jederzeit auf der T-Korrektur-Seite ein- oder ausgeschaltet werden.

---

## 10. Firmware-Update (OTA)

Das Gerät kann über WLAN aktualisiert werden, ohne es vom Bienenstock abbauen zu müssen.

**Vorgehen:**

1. Die neue Firmware mit PlatformIO bauen:
   ```
   pio run
   ```
   Die Datei liegt danach unter: `.pio/build/nodemcuv2/firmware.bin`

2. Webinterface → **OTA**.
3. **Datei auswählen** → `firmware.bin` laden.
4. **Hochladen** klicken.
5. Der Fortschrittsbalken zeigt den Upload-Fortschritt.
6. Nach erfolgreichem Update startet das Gerät automatisch neu.

> **Wichtig:** Das Gerät muss während des Updates mit Strom versorgt bleiben und im WLAN erreichbar sein.

---

## 11. Fehlerbehebung

### Display zeigt nichts an

- Stromversorgung prüfen.
- I²C-Adresse prüfen: Standard ist `0x3F`, manche Displays verwenden `0x27`. Adresse in `src/config.h` anpassen.
- SDA/SCL-Verkabelung prüfen (D1 = SDA, D7 = SCL).

### Gewicht springt stark oder ist instabil

- HX711-Verkabelung prüfen (Wägezellen-Kabel vertauscht?).
- Abschirmung der Wägezellen-Kabel prüfen (siehe Bauanleitung).
- Pufferkondensatoren am HX711 prüfen.
- Gain-Faktor reduzieren (128 → 64).
- Tara erneut setzen (evtl. mechanische Spannung in der Konstruktion).

### Kein WLAN-Verbindung

- SSID und Passwort prüfen.
- Gerät in die Nähe des Routers bringen und erneut versuchen.
- Bei dauerhaftem Fehler: Gerät neu starten → AP-Mode öffnet automatisch nach 10 Minuten.

### MQTT-Verbindung schlägt fehl

- Broker-IP und Port prüfen.
- Sicherstellen, dass Broker und Gerät im gleichen Netzwerk sind.
- Broker-Logs auf Authentifizierungsfehler prüfen.
- Client-ID muss eindeutig sein (kein anderes Gerät mit gleicher ID verbunden).

### Temperaturkorrektur: R² zu niedrig (< 0,5)

- Zu wenig Temperaturvariation in den Daten: längeren Zeitraum wählen.
- Gewicht hat sich während der Aufzeichnung geändert (Fütterung, Ernte): neuen Datensatz aufzeichnen.
- Zu wenige Messpunkte: Aufzeichnungsintervall verkürzen.

### Gerät nicht mehr über WLAN erreichbar

- Router-DHCP-Tabelle prüfen (IP-Adresse kann sich geändert haben).
- Gerät neu starten: kurz Strom trennen.
- Falls Webinterface nicht mehr erreichbar: Gerät gibt nach 10 Minuten ohne Verbindung wieder den AP-Hotspot frei.
