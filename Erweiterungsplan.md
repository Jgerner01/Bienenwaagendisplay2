# Erweiterungsplan: Ertragsmessung + Schnellmessung

## Ziel
Zwei temporäre Tara-Funktionen, die die eigentliche Tara unverändert lassen:
1. **Ertragsmessung** – Langzeit-Referenz für den Stockertrag
2. **Schnellmessung** – Kurzzeit-Messung für aufgelegte Gegenstände

---

## Hardware

- Taster an **SD3 (GPIO10)**, aktiv LOW (Druck = GND verbinden)
- Hinweis: GPIO10 ist ein Flash-Pin; auf NodeMCU/ESP-12E kann er als GPIO verwendet
  werden, sollte aber nicht im SPI-Modus des Flashs liegen.

---

## Tastenlogik (Zustandsautomat)

| Aktion | Ergebnis |
|---|---|
| Kurzer Druck (< 5 s), erster Druck | Schnellmessung aktivieren (Schnell-Tara setzen) |
| Kurzer Druck, Schnellmessung aktiv | Schnellmessung beenden |
| Halten ≥ 5 s, dann loslassen | Bestätigungsfenster öffnen (5 s) |
| Kurzer Druck im Bestätigungsfenster | Ertragsmessung-Tara setzen |
| Kein Druck im Bestätigungsfenster | Abbruch, keine Änderung |

---

## Anzeigemodi LCD 16×2

| Modus | Zeile 1 | Zeile 2 |
|---|---|---|
| Normal (kein Ertrag gesetzt) | Temperatur | Gewicht (korrigiert) |
| Ertragsmessung aktiv | Temperatur | `E:+x.xxx kg` |
| Schnellmessung aktiv | `Schnellmessung  ` | Schnellwert (2 Hz) |
| Bestätigung (5-s-Fenster) | `Ertrag tarieren?` | `Taste->OK (5s)  ` |

---

## Schnellmessung – technische Details

- **Schnell-Puffer**: 10 Samples ≈ 1-Sekunden-Fenster (statt 300)
- **Median** aus diesem Puffer → Schnellwert in kg
- **Display-Update**: 500 ms (2 Hz, statt 1 Hz normal)
- **Ausgabe**: nur LCD, nicht auf Webseite / MQTT

---

## Ertragswert – technische Details

- **Ertragswert** = `weightCorrectedKg − ertragsOffset`
- Wird **nicht** im LittleFS gespeichert (flüchtig, Neustart setzt zurück)
- **Ausgabe**: LCD Zeile 2, Webseite (`/api/data` Feld `ertragsgewicht`), MQTT

### MQTT-Topic (neu)
```
bienenwaage/01/sensors/ertragsgewicht
{"value":0.234,"unit":"kg","ts":12345}
```

---

## Betroffene Dateien

### Neu
| Datei | Inhalt |
|---|---|
| `src/button_handler.h` | ButtonHandler-Klasse, Events-Enum |
| `src/button_handler.cpp` | Zustandsautomat, Entprellung |

### Geändert
| Datei | Änderung |
|---|---|
| `src/config.h` | `BUTTON_PIN 10`, `SCHNELL_SAMPLES 10` |
| `src/scale_reader.h` | `fastWeightKg` in ScaleData, schneller Puffer |
| `src/scale_reader.cpp` | Fast-Median-Berechnung |
| `src/display.h` | `DisplayMode`-Enum, neue `update()`-Signatur |
| `src/display.cpp` | Schnellmessung-Modus, 2-Hz-Update, Bestätigungsanzeige |
| `src/main.cpp` | ButtonHandler einbinden, Tara-Offsets, Modus-Logik |
| `src/webserver.cpp` | `ertragsgewicht` in `/api/data` + Webseite |
| `src/mqtt_client.h` | `publishErtragsData()` Deklaration |
| `src/mqtt_client.cpp` | Ertragsgewicht-Topic + HA-Discovery |
