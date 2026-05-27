# Bauanleitung – Bienenwaagendisplay 2

---

## Inhaltsverzeichnis

1. [Materialliste](#1-materialliste)
2. [Schaltplan und Verdrahtung](#2-schaltplan-und-verdrahtung)
3. [Spannungsversorgung des HX711](#3-spannungsversorgung-des-hx711)
4. [Entstörmaßnahmen gegen Messrauschen](#4-entstörmaßnahmen-gegen-messrauschen)
5. [Aufbau Schritt für Schritt](#5-aufbau-schritt-für-schritt)
6. [Mechanischer Aufbau der Waage](#6-mechanischer-aufbau-der-waage)
7. [Gehäuse und Schutz](#7-gehäuse-und-schutz)
8. [Erstinbetriebnahme](#8-erstinbetriebnahme)

---

## 1. Materialliste

### Elektronik

| Bauteil | Beschreibung | Hinweis |
|---|---|---|
| NodeMCU v2 | ESP8266, ESP-12E-Modul (Amica oder kompatibel) | |
| HX711 | Wägeverstärker-Modul | Mit Breakout-Board |
| Wägezelle | Plattformwaage-Zelle, 50 kg (oder passend zur Beutegröße) | 4-Draht |
| DS18B20 | Temperatursensor, wasserdichtes Edelstahlgehäuse | Für Außeneinsatz |
| LCD 16×2 | I²C-Version mit PCF8574-Backpack | Adresse 0x3F oder 0x27 |
| Widerstand | 4,7 kΩ | Pull-up für DS18B20 |
| Kondensatoren | 10 µF / 100 nF (je 2×) | Entstörung HX711 (siehe Abschnitt 4) |
| Linearregler | z. B. LM1117-3,3 oder AMS1117-3,3 (optional) | Für separate HX711-Versorgung |
| Pegelwandler | Bidirektional, 3,3 V / 5 V (optional) | Alternativ zur 3,3-V-Versorgung |
| Lochrasterplatine | Für die Verdrahtung | |
| Schrauben, Abstandsbolzen | Für die Montage | |
| Wetterfestes Gehäuse | IP65 oder besser | |
| Kabeldurchführungen | PG-Verschraubungen | |

### Werkzeug

- Lötkolben und Lötzinn
- Multimeter
- Seitenschneider, Abisolierzange
- Bohrmaschine (für Gehäuseöffnungen)
- Schraubenzieher

---

## 2. Schaltplan und Verdrahtung

### Pin-Belegung NodeMCU ↔ Peripherie

```
NodeMCU                    Peripherie
─────────────────────────────────────────────────────
D1  (GPIO5)  ────────────── SDA  ─── LCD I²C
D7  (GPIO13) ────────────── SCL  ─── LCD I²C
D2  (GPIO4)  ── 4,7kΩ ─┬── Data ─── DS18B20
                        └── 3,3V
D3  (GPIO0)  ────────────── CK   ─── HX711
D4  (GPIO2)  ────────────── DO   ─── HX711
3,3V         ────────────── VCC  ─── LCD, DS18B20, HX711 *
GND          ────────────── GND  ─── alle Komponenten

* siehe Abschnitt 3 zur HX711-Versorgungsspannung
```

### Wägezelle ↔ HX711

```
Wägezelle (4-Draht)        HX711 Breakout
─────────────────────────────────────────
Rot    (Versorgung +) ───── E+
Schwarz(Versorgung −) ───── E−
Grün   (Signal +)     ───── A+
Weiß   (Signal −)     ───── A−
```

> Die genaue Farbcodierung hängt vom Wägezellen-Hersteller ab. Dokumentation der Zelle beachten. Rot und Schwarz sind fast immer Versorgung, Grün/Weiß oder Blau/Weiß sind das Differenzsignal.

---

## 3. Spannungsversorgung des HX711

### Wichtiger Hinweis zur Betriebsspannung

Der HX711 kann mit 2,7–5,5 V betrieben werden. Da der NodeMCU (ESP8266) mit **3,3 V** arbeitet, gilt:

> **Der HX711 muss ebenfalls mit 3,3 V versorgt werden**, wenn er direkt mit den GPIO-Pins des NodeMCU verbunden ist. Bei Betrieb mit 5 V würden die 5-V-Signale des HX711 (DO-Leitung) direkt auf die 3,3-V-Eingänge des ESP treffen und diesen langfristig beschädigen.

### Option A: HX711 direkt mit 3,3 V (einfachste Lösung)

Den VCC-Pin des HX711 direkt mit dem **3,3V-Pin** des NodeMCU verbinden. Die GPIO-Pegel sind dann kompatibel, kein Pegelwandler nötig.

```
NodeMCU 3,3V ──── HX711 VCC
NodeMCU GND  ──── HX711 GND
```

**Nachteil:** Das HX711-Modul und der ESP teilen sich den 3,3-V-Regler des NodeMCU, der nur begrenzt belastbar ist. Bei Störungen der Versorgungsspannung können sich Messrauschen und Instabilitäten ergeben.

### Option B: Separater Linearregler für den HX711 (empfohlen)

Ein eigener Linearregler (z. B. **LM1117-3,3** oder **AMS1117-3,3**) versorgt den HX711 unabhängig vom NodeMCU:

```
5V (Netzteil) ──── LM1117-3,3 Vin
               └── Cout (10µF + 100nF nach GND)
LM1117 Vout   ──── HX711 VCC
               └── Cin  (10µF + 100nF nach GND)
```

**Vorteile:**
- Saubere, von der ESP-Logik entkoppelte Versorgung
- Stärkere Lastfähigkeit (800 mA)
- Deutlich reduziertes Messrauschen durch Entkopplung

### Option C: Pegelwandler (Variante des Autors)

Bei Verwendung eines **bidirektionalen Pegelwandlers** (3,3 V ↔ 5 V) kann der HX711 mit 5 V betrieben werden:

```
5V-Netzteil ──── HX711 VCC
NodeMCU D3  ──── Pegelwandler Low-Seite A ──── Pegelwandler High-Seite B ──── HX711 CK
NodeMCU D4  ──── Pegelwandler Low-Seite A ──── Pegelwandler High-Seite B ──── HX711 DO
```

**Vorteile:**
- HX711 arbeitet bei seiner Nennspannung (5 V) mit besserem Signal-Rausch-Verhältnis
- ESP-Eingänge sind durch den Pegelwandler geschützt

**Geeignete Module:** BSS138-basierte 4-Kanal-Pegelwandler (weit verbreitet, günstig)

---

## 4. Entstörmaßnahmen gegen Messrauschen

Wägezellen liefern sehr kleine Differenzspannungen (wenige Millivolt). Schon geringe Störeinflüsse können das Messergebnis erheblich beeinflussen. Folgende Maßnahmen reduzieren das Rauschen deutlich:

### 4.1 Abgeschirmtes Kabel für die Wägezelle

Das Kabel zwischen Wägezelle und HX711 sollte **abgeschirmt** (geschirmt) sein:

- Geflochtener oder Folien-Schirm um alle vier Adern
- Den Schirm **einseitig** am GND des HX711 anschließen (nicht beidseitig – sonst Ausgleichsströme)
- Kabel so kurz wie möglich halten
- Kabel nicht parallel zu Netz- oder Versorgungskabeln verlegen

> **Praxistipp:** Fertige Wägezellen-Verlängerungskabel sind oft bereits abgeschirmt und können direkt verwendet werden.

### 4.2 Pufferkondensatoren am HX711

An den Versorgungspins des HX711 Kondensatoren möglichst nahe am Bauteil einlöten:

```
HX711 VCC ──┬── 10 µF (Elektrolyt oder Tantal, niedrige ESR)
             └── 100 nF (Keramik, X7R)
             │
            GND
```

Dasselbe an den Referenz-Pins (falls auf dem Breakout-Board zugänglich):

```
AVDD ──┬── 10 µF
        └── 100 nF
        │
       GND
```

**Wirkung:** Die Kondensatoren puffern kurze Spannungseinbrüche auf der Versorgungsleitung und reduzieren hochfrequente Störungen.

### 4.3 Separater Linearregler (siehe Abschnitt 3, Option B)

Ein eigener Regler für den HX711 entkoppelt die Messelektronik vollständig von den Schaltvorgängen im ESP8266 (WiFi-Sendevorgänge erzeugen kurze, starke Stromspitzen).

### 4.4 Erdung der Konstruktion

- Die mechanische Trägerkonstruktion der Waage (Metall oder leitfähige Teile) mit dem GND verbinden.
- Insbesondere bei Metallkästen oder Blechgehäusen: Gehäuse erden.
- Bei Aufstellung im Freien: Schutz vor elektrostatischer Aufladung durch Erdungsleitung.

### 4.5 Zusammenfassung der Entstörmaßnahmen

| Maßnahme | Aufwand | Wirkung |
|---|---|---|
| Abgeschirmtes Kabel | mittel | hoch – reduziert HF-Einstreuung |
| 100 nF Keramik-C am VCC | gering | mittel – filtert HF-Störungen |
| 10 µF Elko am VCC | gering | mittel – puffert Lastspitzen |
| Separater Linearregler | mittel | hoch – entkoppelt ESP und HX711 |
| Gehäuse und Konstruktion erden | gering | mittel – reduziert Potentialunterschiede |
| Pegelwandler + 5-V-Betrieb | mittel | mittel – besseres SNR des HX711 |

> Mit allen Maßnahmen zusammen sind Streuungen von < 5 g realistisch erreichbar.

---

## 5. Aufbau Schritt für Schritt

### Schritt 1: Wägezelle montieren

Die Wägezelle wird zwischen zwei Platten eingespannt:

```
[ Oberplatte  ]  ← Bienenstock steht hier
      │
   Wägezelle
      │
[ Unterplatte ]  ← liegt auf dem Untergrund
```

- Schrauben laut Datenblatt der Wägezelle anziehen (zu fest verbiegt die Zelle).
- Darauf achten, dass die Zelle frei schwingen kann (keine Berührung am Gehäuse).

### Schritt 2: Verdrahtung Wägezelle → HX711

1. Adern der Wägezelle abisolieren (ca. 5 mm).
2. Laut Farbcodierung (Abschnitt 2) an die Klemmen E+, E−, A+, A− des HX711 anlöten oder anklemmen.
3. Kabelschirm an GND anschließen.

### Schritt 3: HX711 → NodeMCU verdrahten

| HX711-Pin | NodeMCU-Pin |
|---|---|
| VCC | 3,3V (oder Linearregler-Ausgang) |
| GND | GND |
| CK | D3 (GPIO0) |
| DO (DOUT) | D4 (GPIO2) |

Pufferkondensatoren am VCC des HX711 einlöten (Abschnitt 4.2).

### Schritt 4: LCD → NodeMCU verdrahten

| LCD-Pin | NodeMCU-Pin |
|---|---|
| VCC | 3,3V |
| GND | GND |
| SDA | D1 (GPIO5) |
| SCL | D7 (GPIO13) |

### Schritt 5: DS18B20 → NodeMCU verdrahten

| DS18B20-Pin | NodeMCU-Pin |
|---|---|
| VCC (rot) | 3,3V |
| GND (schwarz) | GND |
| Data (gelb/weiß) | D2 (GPIO4) |

4,7-kΩ-Widerstand zwischen Data und 3,3V einlöten (Pull-up).

### Schritt 6: Spannungsversorgung

- NodeMCU über Micro-USB mit einem 5-V-Netzteil (min. 500 mA) versorgen.
- Alternativ: 5-V-Einspeisung über VIN-Pin.
- Bei separatem Linearregler: 5-V-Eingang auch für den HX711-Regler verwenden.

### Schritt 7: Funktionstest vor dem Einbau

Vor dem Einbau ins Gehäuse:

1. Firmware flashen (USB-Kabel, PlatformIO).
2. Seriellen Monitor öffnen (115200 Baud) – Startmeldungen prüfen.
3. LCD-Anzeige prüfen (erscheint nach wenigen Sekunden).
4. WiFi einrichten (Abschnitt 8 / Bedienungsanleitung).
5. Webinterface öffnen → Wägezellen-Rohwert prüfen (sollte sich ändern wenn Gewicht aufgelegt wird).

---

## 6. Mechanischer Aufbau der Waage

### Konstruktionsprinzipien

- Die Wägezelle muss **zentriert** unter dem Bienenstock sitzen.
- Ober- und Unterplatte sollten **steif** sein (mind. 18 mm Siebdruckplatte oder Aluminiumprofil).
- Die Zelle darf **keine Querkräfte** aufnehmen – der Stock muss gerade aufgestellt sein.
- Metallschrauben der Halterung nicht zu stark anziehen (Verbiegen der Zelle vermeiden).

### Schutz der Wägezelle

- Wägezelle mit **Schrumpfschlauch** oder **Silikon** gegen Feuchtigkeit schützen.
- Kabel durch PG-Verschraubungen aus dem Gehäuse führen.
- Kabeldurchführungen abdichten (Silikon).

### Empfohlene Plattengröße

- **Unterplatte:** ca. 400 × 400 mm (passt zur Standard-Zander-Beute)
- **Oberplatte:** gleiche Größe oder etwas kleiner
- **Materialstärke:** mind. 18 mm (Siebdruckplatte witterungsbeständig)

---

## 7. Gehäuse und Schutz

### Anforderungen

- **Schutzklasse:** IP65 oder besser (staubdicht, strahlwasserdicht)
- **UV-beständig** (Außeneinsatz)
- Ausreichend groß für NodeMCU, HX711, ggf. Linearregler und Lochrasterplatine

### Lüftung

Das Gehäuse sollte **nicht hermetisch** abgedichtet sein – Kondenswasser kann sonst Elektronik beschädigen. Optionen:

- Kleines Belüftungsgitter an der Unterseite (Insektenschutz beachten)
- Goretex-Membran-Entlüftungsventil (IP65 mit Druckausgleich)

### LCD-Durchführung

Das Display sollte von außen ablesbar sein:

- Ausschnitt ins Gehäuse sägen/fräsen
- Display von innen mit Dichtungsrahmen oder Silikon abdichten
- Alternativen: Gehäuse mit Sichtfenster, oder nur MQTT ohne Display im Außeneinsatz

---

## 8. Erstinbetriebnahme

Nach dem mechanischen und elektrischen Aufbau:

1. **Firmware flashen** (einmalig per USB-Kabel, danach OTA möglich).
2. **WiFi einrichten** (Bedienungsanleitung, Abschnitt 3).
3. **Tara setzen** – leere Beute auf die Waage stellen, Tara im Webinterface setzen.
4. **Kalibrieren** – bekanntes Gewicht auflegen, Wert eingeben, kalibrieren.
5. **MQTT konfigurieren** (optional) – Broker-Daten eingeben, speichern, Neustart.
6. **Temperaturkorrektur** (optional, nach mehreren Tagen Laufzeit) – CSV exportieren, Koeffizienten berechnen, speichern.

---

## Hinweise zur Langzeitstabilität

- Wägezellen-Kabel **nicht knicken** – Ermüdungsbrüche nach Wochen möglich.
- **Kondensatoren** regelmäßig auf Korrosion prüfen (Außeneinsatz).
- Nach dem Winter: **Tara erneut setzen** (mechanische Entspannung der Konstruktion über den Winter).
- **Firmware aktuell halten** – OTA-Update aus dem Webinterface, kein Werkzeug nötig.
