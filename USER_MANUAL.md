# User Manual – Beehive Scale Display 2

---

## Table of Contents

1. [Overview](#1-overview)
2. [Display Modes](#2-display-modes)
3. [Button – Yield Measurement and Quick Measurement](#3-button--yield-measurement-and-quick-measurement)
4. [First Start – WiFi Setup](#4-first-start--wifi-setup)
5. [WiFi Auto-Reconnect and Reboot](#5-wifi-auto-reconnect-and-reboot)
6. [Accessing the Web Interface](#6-accessing-the-web-interface)
7. [Section: Scale](#7-section-scale)
8. [Section: Parameters](#8-section-parameters)
   - [8.1 Setting the Tare](#81-setting-the-tare)
   - [8.2 Calibration](#82-calibration)
   - [8.3 Gain Factor](#83-gain-factor)
   - [8.4 Poly2 Temperature Correction (Stage 1)](#84-poly2-temperature-correction-stage-1)
   - [8.5 PT2 Correction (Stage 2)](#85-pt2-correction-stage-2)
9. [MQTT Configuration](#9-mqtt-configuration)
10. [Firmware Update (OTA)](#10-firmware-update-ota)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Overview

The beehive scale continuously measures the weight of the hive and ambient temperature. Values are shown on an LCD display and optionally transmitted via MQTT to a smart-home system (e.g. Home Assistant).

**Features at a glance:**

| Feature | Description |
|---|---|
| Weight measurement | HX711 load cell amplifier, ±1 g resolution |
| Temperature measurement | DS18B20, ±0.5 °C |
| Poly2 T-correction | Compensates temperature-induced weight drift (Stage 1) |
| PT2 correction | PT2-filtered temperature correction (Stage 2) |
| Yield measurement | Long-term reference for hive yield, via button or web |
| Quick measurement | Short-term measurement for placed objects, via button |
| Display | LCD 16×2 with three display modes |
| Web interface | Configuration and calibration via browser |
| MQTT | Data transfer to Home Assistant or other brokers |
| OTA | Firmware update over WiFi |
| Auto-reconnect | Automatic reconnection after WiFi loss |

---

## 2. Display Modes

The display has three modes, switched by the button (D8).

### Normal mode

```
T:  18.8 C
   26.547 kg
```

Line 1: Temperature | Line 2: corrected weight (or yield value if set)

```
T:  18.8 C
E: +0.235 kg
```

When yield measurement is active, line 2 shows the yield value (current weight minus yield base, with sign).

### Quick measurement mode

```
Schnellmessung
  +0.567 kg
```

Updated 2× per second. Shows weight relative to the quick-tare.

### Confirmation mode (setting yield base)

```
Ertrag tarieren?
Taste->OK (5s)
```

Appears after a long button press (≥ 5 s). Press briefly within 5 s to confirm.

**At startup**, the IP address is briefly shown:

```
Bienenwaage
192.168.1.42
```

In AP mode:

```
Bienenwaage AP
192.168.4.1
```

---

## 3. Button – Yield Measurement and Quick Measurement

A button is connected to **D8 (GPIO15)** with an external **10 kΩ pull-down resistor** to GND. Pressing connects D8 to 3.3 V.

### Quick measurement

| Action | Result |
|---|---|
| Short press (< 5 s) | Set quick-tare, start quick measurement |
| Another short press | End quick measurement |

**Use:** Place an object on the hive → short press → display shows the object's weight relative to the quick-tare.

### Yield measurement

| Action | Result |
|---|---|
| Hold ≥ 5 s, then release | Confirmation window opens (5 s) |
| Short press within window | Set yield base to current weight |
| No press within window | Cancelled, no change |

**Use:** At the start of the season or after winterisation: long press, then confirm. Line 2 will then show the weight gain since that point.

> **The yield base is stored persistently** and survives power cycles.

> Yield measurement can also be set and reset via the web interface under **Scale**.

---

## 4. First Start – WiFi Setup

On first startup the device opens its own WiFi hotspot:

| Setting | Value |
|---|---|
| SSID | `Bienenwaage` |
| Password | `12345678` |
| IP address | `192.168.4.1` |

**Steps:**

1. Connect smartphone or PC to `Bienenwaage` WiFi.
2. Open browser → `192.168.4.1`.
3. Tap **WiFi** → select home network → enter password → **Connect**.
4. The new IP address appears on the display.
5. Connect to the home network and open the shown IP in a browser.

> **Tip:** The current SSID is pre-filled in the WiFi form. Leave the password field empty to keep the stored password unchanged.

> The configuration hotspot (192.168.4.1) remains accessible for 2 minutes while the device is already on the home network.

---

## 5. WiFi Auto-Reconnect and Reboot

The device automatically tries to restore the connection after a drop:

| Phase | Description |
|---|---|
| Connection lost | Immediate first reconnect attempt |
| Reconnect attempts | Every 30 seconds, up to 10 times |
| After 10 failures (5 min) | Switch to AP mode (192.168.4.1) |
| 10 min in AP mode without connection | Automatic reboot |

After reboot the device reconnects to the stored network. If reachable, normal operation resumes. Otherwise the cycle repeats.

---

## 6. Accessing the Web Interface

```
http://<IP-address>/
```

The IP address appears briefly on the display at startup, or can be found in the router's DHCP table (hostname: `bienenwaage`).

**Navigation:**

| Menu item | Function |
|---|---|
| Scale | Measurements (3 weight values), yield measurement set/reset |
| Parameters | Tare, calibration, gain, Poly2 T-correction, PT2 correction |
| MQTT | Configure MQTT broker |
| WiFi | Change network |
| OTA | Firmware update |

---

## 7. Section: Scale

The Scale page shows three weight values:

| Value | Description |
|---|---|
| Weight (raw) | Uncompensated measurement |
| Weight (T-corr.) | After Poly2 temperature correction (Stage 1), `*` when active |
| Weight (T+PT2-corr.) | After Poly2 + PT2 correction (Stages 1+2), `**` when active |

Additionally:

- **Yield value**: weight relative to the set yield base
- **Set yield base now**: sets the current fully-corrected weight as the reference
- **Reset yield measurement**: deactivates yield measurement

---

## 8. Section: Parameters

All measurement parameters are editable on a single page: **Web interface → Parameters**.

### 8.1 Setting the Tare

Compensates the dead weight of the hive components.

**When to set?** After initial assembly, after adding/removing hive parts, or if the displayed weight deviates significantly from zero without reason.

1. Place all hive components on the scale (without bees/honey if desired).
2. **Parameters → Set tare**.

> The tare value is stored permanently.

### 8.2 Calibration

**Prerequisite:** Tare set, known reference weight (e.g. 5–20 kg).

1. Place the reference weight on the scale.
2. **Parameters → enter known weight** (e.g. `5.250`).
3. Click **Calibrate**.

> Use a weight close to the typical hive weight for best accuracy.

### 8.3 Gain Factor

| Gain | Channel | Recommendation |
|---|---|---|
| 128 | A | Default – highest resolution (preset) |
| 64 | A | If overdriven at Gain 128 |
| 32 | B | Second input (if wired) |

**Parameters → select gain → Save.**

> Repeat calibration after changing the gain factor.

### 8.4 Poly2 Temperature Correction (Stage 1)

Corrects temperature-induced weight drift with a 2nd-order polynomial:

```
correction(T) = a·T² + b·T + c
Weight (T-corr.) = raw weight − correction(T)
```

**Enter coefficients manually:** Parameters → Poly2 T-Correction → a, b, c → Save.

**Calculate coefficients from measurement data (recommended):**

1. Record data over a temperature range of ≥ 10 °C (constant weight!).
2. Export CSV files from Home Assistant (weight + temperature).
3. **Parameters → CSV Fit Wizard** (links to `/tempcal`).
4. Upload both CSV files → **Calculate**.
5. Check R² (aim for > 0.8) → **Save**.

### 8.5 PT2 Correction (Stage 2)

The PT2 correction accounts for thermally slow effects: temperature is first passed through a PT2 low-pass filter, then a Poly2 is applied to the filtered temperature:

```
T_pt2  = PT2 filter(T, T₂, D)
correction(T_pt2) = a·T_pt2² + b·T_pt2 + c
Weight (T+PT2-corr.) = Weight (T-corr.) − correction(T_pt2)
```

**Parameters:**

| Parameter | Description | Typical value |
|---|---|---|
| T₂ (minutes) | PT2 filter time constant | 120–480 min |
| D (damping) | 0.5 = oscillatory, 0.7 = critically damped, 1.0 = overdamped | 0.7 |
| a, b, c | Poly2 coefficients for the filtered temperature | from CSV fit |

**Calculate coefficients:**

1. **Parameters → CSV Fit Wizard (PT2)** (links to `/pt2cal`).
2. Set T₂ and D.
3. Upload CSV files → **Calculate**.
4. The browser applies the PT2 filter to the temperature and fits Poly2.
5. Check R², adjust T₂ if needed → **Save**.

> Stages 1 and 2 can be enabled independently. For most applications Stage 1 (direct Poly2) is sufficient.

---

## 9. MQTT Configuration

**Prerequisite:** MQTT broker in the network (e.g. Mosquitto).

**Web interface → MQTT:**

| Field | Example | Description |
|---|---|---|
| Broker | `192.168.1.10` | Broker IP address |
| Port | `1883` | Default port |
| Username | (optional) | Authentication |
| Password | (optional) | |
| Client ID | `bienenwaage_01` | Unique device ID |
| Topic prefix | `bienenwaage/01` | Prefix for all topics |
| Publish interval | `60` | Seconds |
| MQTT enabled | Yes | |
| HA Auto-Discovery | Yes | Auto-create sensors in Home Assistant |

**Published values:**

| Topic | Content |
|---|---|
| `.../sensors/weight` | Raw weight in kg |
| `.../sensors/weight_t_corrected` | Weight after Poly2 T-correction (Stage 1) |
| `.../sensors/weight_corrected` | Weight after T+PT2 correction (Stages 1+2) |
| `.../sensors/ertragsgewicht` | Yield value in kg (only when yield measurement is active) |
| `.../sensors/temperature` | Temperature in °C |
| `.../sensors/trimmedmean` | Trimmed mean weight in kg |
| `.../sensors/spread` | Standard deviation in kg |
| `.../sensors/raw` | HX711 raw value |
| `.../status` | `online` / `offline` |

---

## 10. Firmware Update (OTA)

1. Build the firmware:
   ```
   pio run
   ```
   File location: `.pio/build/nodemcuv2/firmware.bin`

2. **Web interface → OTA → Choose file → Upload.**
3. After a successful update the device restarts automatically.

> Only `firmware.bin` is needed for OTA – not `firmware.elf`.

> Keep the device powered throughout the update.

---

## 11. Troubleshooting

### Display shows nothing

- Check power supply.
- I²C address: default `0x3F`, some displays use `0x27` (adjust in `src/config.h`).
- Check SDA/SCL wiring: D1 = SDA, D7 = SCL.

### Weight unstable or jumping

- Check HX711 wiring.
- Reduce gain factor (128 → 64).
- Re-set tare.

### No WiFi – device unreachable

- The device retries every 30 s automatically (up to 10 times).
- After 10 failures it switches to AP mode (`Bienenwaage`, 192.168.4.1).
- After 10 min in AP mode without connection: **automatic reboot**.
- Re-enter SSID and password on the WiFi page → **Connect**.

### Yield value missing after reboot

- The yield base is saved in `/ertrag.json` and survives reboots.
- If still missing: check LittleFS storage (full reconfiguration via `uploadfs`).

### Temperature correction: R² too low (< 0.5)

- Insufficient temperature variation: use a longer time period (≥ 10 °C range).
- Weight changed during recording: record a new dataset.
- For PT2: adjust T₂ and recalculate.

### MQTT connection fails

- Check broker IP and port.
- Device and broker must be on the same network.
- Client ID must be unique.

### Button not responding

- External 10 kΩ pull-down resistor required between D8 (GPIO15) and GND.
- Button connects D8 to 3.3 V when pressed.
