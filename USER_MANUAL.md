# User Manual – Beehive Scale Display 2

---

## Table of Contents

1. [Overview](#1-overview)
2. [Display](#2-display)
3. [First Start – WiFi Setup](#3-first-start--wifi-setup)
4. [Accessing the Web Interface](#4-accessing-the-web-interface)
5. [Setting the Tare](#5-setting-the-tare)
6. [Calibration](#6-calibration)
7. [Gain Factor](#7-gain-factor)
8. [MQTT Configuration](#8-mqtt-configuration)
9. [Temperature Correction](#9-temperature-correction)
10. [Firmware Update (OTA)](#10-firmware-update-ota)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Overview

The beehive scale continuously measures the weight of the beehive and the ambient temperature. Values are shown on an LCD display and optionally transmitted via MQTT to a smart-home system (e.g. Home Assistant).

**Features at a glance:**

| Feature | Description |
|---|---|
| Weight measurement | HX711 load cell amplifier, ±1 g resolution |
| Temperature measurement | DS18B20, ±0.5 °C |
| Temperature correction | Automatic compensation of temperature-induced weight drift |
| Display | LCD 16×2: temperature + weight |
| Web interface | Configuration and calibration via browser |
| MQTT | Data transfer to Home Assistant or other brokers |
| OTA | Firmware update over WiFi |

---

## 2. Display

After startup, the LCD shows two lines:

```
T:  18.8 C
   26.547 kg
```

| Line | Content |
|---|---|
| Line 1 | Current temperature in °C |
| Line 2 | Current weight in kg (temperature-corrected if active) |

**On power-up**, the IP address is shown for approx. 5 seconds:

```
Bienenwaage
192.168.1.42
```

If no WiFi is configured:

```
Bienenwaage AP
192.168.4.1
```

---

## 3. First Start – WiFi Setup

On first startup (no WiFi credentials stored), the device opens its own hotspot:

| Setting | Value |
|---|---|
| SSID | `Bienenwaage` |
| Password | `12345678` |
| IP address | `192.168.4.1` |

**Steps:**

1. Connect your smartphone or PC to the `Bienenwaage` WiFi network.
2. Open a browser and navigate to `192.168.4.1`.
3. Tap **WiFi** in the navigation menu.
4. Select your home network from the list, enter the password, click **Connect**.
5. The device connects to your home network. The new IP address appears on the display and in the browser.
6. Switch back to your home network and open the displayed IP address in your browser.

> **Tip:** The configuration access point (192.168.4.1) stays active for 2 more minutes while the device is already connected to your home network.

---

## 4. Accessing the Web Interface

The web interface is accessible from any browser on the same network:

```
http://<IP address>/
```

The IP address is shown on the display (briefly after startup) or in your router's device list (hostname: `bienenwaage`).

**Navigation:**

| Menu item | Function |
|---|---|
| Scale | Details: raw value, offset, calibration factor, tare |
| Calibration | Calibrate weight and reset to defaults |
| Gain | Set HX711 amplification factor |
| T-Correction | Calculate and enable temperature correction |
| MQTT | Configure MQTT broker |
| WiFi | Switch network |
| OTA | Firmware update |

---

## 5. Setting the Tare

The tare (zero point) compensates for the dead weight of the hive components.

**When to tare:**
- On first setup, after all hive parts are placed on the scale.
- After adding or removing hive components.
- When the displayed weight deviates noticeably from zero without any actual change.

**Steps:**

1. Place all hive components on the scale (ideally without bees and honey if you want to track net weight).
2. Open the web interface → **Scale** → **Set Tare**.
3. The device stores the current raw value as zero. The display switches to 0.000 kg.

> The tare value is stored permanently and survives power cycles.

---

## 6. Calibration

Calibration ensures that the displayed weight matches the actual weight.

**Required:** A known reference weight (e.g. a stone weighed on a calibrated scale beforehand).

**Steps:**

1. The scale must be tared first (see Step 5).
2. Place the reference weight on the scale.
3. Open the web interface → **Calibration**.
4. Enter the known weight in kg (e.g. `5.250`).
5. Click **Calibrate**.
6. The new calibration factor is saved and applied immediately.

> **Tip:** Use a reference weight close to the typical hive weight (5–50 kg) for best accuracy.

**Resetting to factory defaults:**

Web interface → **Calibration** → **Reset to Factory Settings** resets calibration factor, offset and gain to defaults.

---

## 7. Gain Factor

The gain factor determines the input channel and amplification of the HX711.

| Gain | Channel | Recommendation |
|---|---|---|
| 128 | A | Default – best resolution (pre-set) |
| 64 | A | If measurement range is exceeded at Gain 128 |
| 32 | B | Second input (if wired) |

Web interface → **Gain** → select value → **Save**.

> After changing the gain factor, repeat the calibration.

---

## 8. MQTT Configuration

MQTT enables data transmission to Home Assistant or other smart-home systems.

**Prerequisite:** An MQTT broker on the network (e.g. Mosquitto).

**Steps:**

1. Web interface → **MQTT**.
2. Fill in the following fields:

| Field | Example | Description |
|---|---|---|
| Broker | `192.168.1.10` | IP address of the MQTT broker |
| Port | `1883` | Standard MQTT port |
| Username | (optional) | If the broker requires authentication |
| Password | (optional) | |
| Client ID | `bienenwaage_01` | Unique device identifier |
| Topic prefix | `bienenwaage/01` | Prepended to all topics |
| Publish interval | `60` | Seconds between publications |
| MQTT enabled | Yes | |
| HA Auto-Discovery | Yes | Sensors are automatically created in Home Assistant |

3. Click **Save**, then restart the device.

**Published values:**

| Topic | Content |
|---|---|
| `.../sensors/weight` | Raw weight in kg |
| `.../sensors/weight_corrected` | Temperature-corrected weight in kg |
| `.../sensors/temperature` | Temperature in °C |
| `.../sensors/trimmedmean` | Trimmed mean weight in kg |
| `.../sensors/spread` | Standard deviation in kg |
| `.../status` | `online` / `offline` |

---

## 9. Temperature Correction

Load cells change their reading with temperature. The temperature correction compensates this effect using a 2nd-order polynomial:

```
corrected_weight = raw_weight − (a·T² + b·T + c)
```

### 9.1 Recording Data

For a good calibration, data over a **temperature range of at least 10 °C** is needed — e.g. over several days or across a full day-night cycle.

**Important:** The **actual weight must not change** during the recording period (e.g. use an empty hive, or make sure no harvesting or feeding takes place).

### 9.2 Exporting CSV Files from Home Assistant

1. Open Home Assistant → **History**.
2. Select the desired time period and entity:
   - Once for weight (`sensor.bienenwaage_..._gewicht`)
   - Once for temperature (`sensor.bienenwaage_..._temperatur`)
3. Click the download icon for each → save the CSV file.

### 9.3 Calculating Coefficients

1. Web interface → **T-Correction**.
2. Select the **Weight CSV** file.
3. Select the **Temperature CSV** file.
4. Click **Calculate**.
5. The result is displayed:
   - Coefficients a, b, c of the correction polynomial
   - R² value (goodness of fit – should be > 0.8)
   - Chart with data points and fitted curve
6. Choose whether to activate the correction (Yes/No).
7. Click **Save**.

### 9.4 Checking the Result

After saving, the web interface home page shows both values:

- **Weight (raw):** uncompensated measurement
- **Weight (corrected) \*:** temperature-compensated value (highlighted green when active)

The corrected weight is also published to MQTT under `.../sensors/weight_corrected`.

> The correction can be toggled on or off at any time on the T-Correction page.

---

## 10. Firmware Update (OTA)

The device can be updated over WiFi without removing it from the hive.

**Steps:**

1. Build the new firmware with PlatformIO:
   ```
   pio run
   ```
   The file is located at: `.pio/build/nodemcuv2/firmware.bin`

2. Web interface → **OTA**.
3. Click **Choose file** → select `firmware.bin`.
4. Click **Upload**.
5. The progress bar shows the upload status.
6. After a successful update the device restarts automatically.

> **Important:** The device must remain powered and reachable on the WiFi network during the update.

---

## 11. Troubleshooting

### Display shows nothing

- Check power supply.
- Check I²C address: default is `0x3F`, some displays use `0x27`. Adjust `LCD_I2C_ADDR` in `src/config.h`.
- Check wiring: D1 = SDA, D7 = SCL.

### Weight jumps or is unstable

- Check HX711 wiring (load cell cables swapped?).
- Check shielding of load cell cables (see assembly guide).
- Check buffer capacitors on the HX711.
- Reduce gain factor (128 → 64).
- Re-tare the scale (possible mechanical tension in the construction).

### No WiFi connection

- Check SSID and password.
- Move device closer to the router and try again.
- In case of permanent failure: restart device → AP mode opens automatically after 10 minutes without connection.

### MQTT connection fails

- Check broker IP and port.
- Ensure broker and device are on the same network.
- Check broker logs for authentication errors.
- Client ID must be unique (no other device connected with the same ID).

### Temperature correction: R² too low (< 0.5)

- Not enough temperature variation in the data: choose a longer time period.
- Weight changed during recording (feeding, harvesting): record a new dataset.
- Too few data points: reduce the recording interval.

### Device no longer reachable via WiFi

- Check the router's DHCP table (IP address may have changed).
- Restart the device: briefly disconnect power.
- If the web interface is no longer accessible: after 10 minutes without connection, the device opens the AP hotspot again.
