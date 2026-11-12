# Off-Grid Biometric Ankle Tracker (Split-Node Wearable)

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20DevKit%20V1-blue.svg)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino%20%2F%20C%2B%2B-00979D.svg)](https://www.arduino.cc/)
[![Network: SoftAP](https://img.shields.io/badge/Network-Off--Grid%20Wi--Fi%20SoftAP-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An academic-grade, split-node wearable biometric tracker engineered specifically to bypass the inaccuracies and momentum artifacts ("floppy effect") of wrist-based trackers. The device pairs bare-metal sensor polling with a local, zero-dependency dark-mode web dashboard served autonomously over an ESP32 Wi-Fi SoftAP at `192.168.4.1`.

---

## 1. Architectural Blueprint: The Split-Node System

Traditional wearable trackers worn at the wrist experience severe false positives (typing, gesturing) and false negatives during locomotion. Mounting the entire payload directly on the ankle introduces data-corrupting momentum forces and crush vulnerability.

This design divides the hardware into two distinct physical zones connected by a **5-wire strain-relieved harness** ($< 6\text{–}8\text{ inches}$ to adhere strictly to I2C capacitance thresholds):

```
+-------------------------------------------------------------------------+
|                      POD A: MAIN PROCESSING POD                         |
|             (Mounted to Shoelaces or Lower Calf Band)                   |
|                                                                         |
|  [ESP32 DevKit V1] <---> [LiPo Battery / TP4056 Power Mgmt]             |
|         |                                                               |
|         +---> [BMP280 Barometric Altimeter] (I2C 0x76 / 0x77)           |
|         +---> [DHT22 / DHT11 Microclimate RH Sensor] (GPIO 4)           |
+-------------------------------------------------------------------------+
                                    |
                    5-WIRE STRAIN-RELIEVED HARNESS
                    [VCC (3.3V) | GND | SDA | SCL | SIG]
                                    |
+-------------------------------------------------------------------------+
|                    POD B: BIOMETRIC SENSOR NODE                         |
|            (Stitched to Sock Elastic Cuff at Ankle Joint)               |
|                                                                         |
|  * Outer Ankle Bone: [MPU6050 6-Axis IMU] (I2C 0x68)                    |
|    - Captures ground-impact deceleration vectors for pedometry.         |
|                                                                         |
|  * Inner Ankle Bone: [Y1708 Optical PPG Pulse Sensor] (Analog GPIO 32)  |
|    - Secure elastic compression over posterior tibial capillary bed.    |
+-------------------------------------------------------------------------+
```

---

## 2. Pinout & 5-Wire Harness Specification

### Main ESP32 DevKit V1 Pin Assignment

| Peripheral / Sensor | Signal | ESP32 GPIO | Bus / Protocol | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **MPU6050 & BMP280** | SDA | `GPIO 21` | I2C (400 kHz) | Shared I2C data bus |
| **MPU6050 & BMP280** | SCL | `GPIO 22` | I2C (400 kHz) | Shared I2C clock bus |
| **Y1708 PPG Pulse** | Analog Signal | `GPIO 32` | ADC1 Ch 4 | Safe for concurrent Wi-Fi ops |
| **DHT11 / DHT22** | Microclimate Data| `GPIO 4` | Digital Single-Wire | Skin-facing orientation |
| **Onboard Status LED** | Pulse Indicator | `GPIO 2` | Digital Output | Blinks on detected heartbeat |
| **Power Bus** | VCC | `3V3` or `VIN` | Power | Regulated 3.3V DC rail |
| **Ground Bus** | GND | `GND` | Common Ground | Star ground configuration |

### 5-Wire Inter-Node Harness Table

| Wire # | Designation | Color (Standard) | Connection (Pod A &rarr; Pod B) | Function |
| :---: | :--- | :--- | :--- | :--- |
| **1** | `VCC` | Red | ESP32 `3V3` &rarr; MPU6050 `VCC` & Y1708 `VCC` | 3.3V Sensor Power Rail |
| **2** | `GND` | Black | ESP32 `GND` &rarr; MPU6050 `GND` & Y1708 `GND` | Common Ground Plane |
| **3** | `SDA` | Blue / Green | ESP32 `GPIO 21` &rarr; MPU6050 `SDA` | I2C Serial Data |
| **4** | `SCL` | Yellow / White | ESP32 `GPIO 22` &rarr; MPU6050 `SCL` | I2C Serial Clock |
| **5** | `PPG_SIG` | Purple / Orange | ESP32 `GPIO 32` &larr; Y1708 `Signal` | Optical Photoplethysmogram ADC |

---

## 3. Mathematical Algorithms & Firmware Implementation

### A. MPU6050 Footfall & Pedometer Logic
Human gait generates ground-reaction forces predominantly below 5 Hz. To eliminate arm swing and jitter:
1. Calculates instantaneous 3D acceleration magnitude:
   $$\|A\| = \sqrt{a_x^2 + a_y^2 + a_z^2}$$
2. Subtracts the $1g$ ($9.80665 \text{ m/s}^2$) static Earth gravity baseline:
   $$A_{\text{net}} = |\|A\| - g|$$
3. Applies a peak-threshold trigger of $2.20 \text{ m/s}^2$ with a strict **350 ms hysteresis lockout window** ($t_{\text{now}} - t_{\text{last}} \ge 350\text{ ms}$) to prevent false double-counts during foot rolling.

### B. Y1708 Optical PPG Heart Rate Detection
1. Sampled at 100 Hz on `ADC1_CH4` (`GPIO 32`), which operates without interference from the ESP32 Wi-Fi hardware (unlike ADC2).
2. Dynamic DC baseline tracking using an Exponentially Weighted Moving Average (EWMA, $\alpha = 0.95$):
   $$\text{DC}_t = \alpha \cdot \text{DC}_{t-1} + (1 - \alpha) \cdot \text{Raw}_t$$
3. AC-coupled optical signal extraction: $\text{AC}_t = \text{Raw}_t - \text{DC}_t$.
4. Rising-edge dynamic threshold trigger with beat-to-beat Inter-Beat-Interval ($IBI$) timing:
   $$\text{BPM} = \frac{60000}{\text{IBI (ms)}}$$
5. Filtered through an outlier clamp ($45 \le \text{BPM} \le 215$) and buffered in a 10-sample moving average.

### C. BMP280 Elevation Gain & Downhill Filter
1. Polled at 1 Hz with ultra-high-resolution 16x oversampling.
2. **0.5-meter deadband filter**: Changes $|\Delta h| < 0.5\text{ m}$ are discarded to prevent false altitude drift caused by wind drafts and turbulence.
3. **Downhill filter**: Only positive altitude changes ($\Delta h > 0$) accumulate to `elevationGain`. Descents do not subtract from vertical work accomplished.

### D. DHT Microclimate Perspiration Index
1. The sensor faces inward against the skin, capturing the microclimate trapped by the sock.
2. Computes the **Delta Relative Humidity ($\Delta\text{RH}$)**:
   $$\Delta\text{RH} = \text{RH}_{\text{skin}} - \text{RH}_{\text{baseline}}$$
3. Computes the **Sweat Rate** as a 60-second rolling derivative:
   $$\text{Sweat Rate} = \frac{\Delta\text{RH}}{\Delta t_{\text{minutes}}} \quad [\%/\text{min}]$$
4. Categorizes hydration state into actionable tiers: *Basal/Dry*, *Mild Perspiration*, *Active Sweat*, or *Heavy Sweat / Hydrate Now*.

### E. Physics-Based Caloric Expenditure
Combines horizontal kinematic work with vertical mechanical work against gravity, scaled to athlete body mass:
$$\text{Calories}_{\text{Total}} = \left( \text{Steps} \times 0.040 + \text{Elevation Gain (m)} \times 0.150 \right) \times \left(\frac{\text{Weight}_{\text{kg}}}{70.0}\right)$$

---

## 4. Off-Grid Mobile Web Dashboard

The ESP32 broadcasts a standalone local Wi-Fi SoftAP:
* **SSID:** `ESP32_BioTracker`
* **Password:** `biotracker123`
* **Static Dashboard IP:** `http://192.168.4.1`

### Key Web Features
* **100% Off-Grid & Self-Contained:** Zero external CDN dependencies (no Google Fonts, FontAwesome, or external scripts). Fully functional in deep wilderness without internet connectivity.
* **Real-Time AJAX Streaming:** JavaScript `fetch()` routine polls `/api/data` every 1,000 ms to seamlessly update all biometrics without page reloads.
* **Real-Time Canvas Trend Chart:** Native HTML5 canvas renders live perspiration and motion trends.
* **Two-Way Flash NVS Synchronization:** User can adjust Height and Weight in the mobile UI and push changes directly into ESP32 non-volatile flash memory (`Preferences.h`).
* **Session Controls:** Direct buttons to Zero Elevation, Calibrate Ambient Humidity Baseline, and Reset Session Step Counters.

---

## 5. Academic Citations & Scientific Justification

| Research Focus | Academic Citation | Key Finding / Design Validation |
| :--- | :--- | :--- |
| **Lower-Limb Pedometer Placement** | *Nature Scientific Reports* (2024). [Accuracy and reliability of accelerometer-based pedometers](https://pubmed.ncbi.nlm.nih.gov/39532924/) | Proves lower-limb placement significantly outperforms wrist placement by removing incidental arm movement. |
| **Dead-Reckoning Gait Kinematics** | *MDPI Sensors* (2018). [Height-Adaptive Step Length Measurement](https://www.mdpi.com/1424-8220/18/4/1039) | Validates MPU6050 lower-limb mounting and parameterized step-length calculations. |
| **Gait Frequency Characteristics** | *MDPI Sensors* (2025). [Every Step Counts: Accurately Counting Steps](https://www.mdpi.com/1424-8220/25/18/5657) | Confirms human locomotion acceleration concentrates $< 5\text{ Hz}$, justifying the 350 ms debounce threshold. |
| **Textile PPG Integration** | *MDPI Electronics* (2023). [Inexpensive Optical Pulse Sensors in Textiles](https://www.mdpi.com/2079-9292/12/7/1521) | Validates stabilizing optical PPG sensors under elastic cuff pressure against the ankle bone. |
| **Vertical Work Energy Expenditure** | *Int J Sport Nutr Exerc Metab* (2014). [SenseWear Mini Armband Assessment](https://pubmed.ncbi.nlm.nih.gov/25309985/) | Demonstrates traditional wearables underestimate uphill caloric burn without barometric elevation sensors. |
| **Barometric Wearable Accuracy** | *Sensors* (2020). [Accurate Spirometry with Barometric Sensors](https://pmc.ncbi.nlm.nih.gov/articles/PMC7435382/) | Validates the Bosch BMP280 as a reliable, miniature wearable altimeter with high thermal stability. |
| **Microclimate Hydration Monitoring** | *MDPI Sensors* (2026). [Wearable Sensing for Personal Thermal Comfort](https://www.mdpi.com/1424-8220/26/17/5640) | Validates DHT-series relative humidity monitoring for localized skin microclimate perspiration tracking. |

---

## 6. How to Build & Flash

### Option A: Using PlatformIO (Recommended)
1. Install [PlatformIO IDE](https://platformio.org/) in VS Code or Antigravity.
2. Open this directory (`s&i project`).
3. Connect your ESP32 DevKit V1 via Micro-USB.
4. Run the upload command:
   ```bash
   pio run --target upload
   pio device monitor
   ```

### Option B: Using Arduino IDE
1. Install **ESP32 Board Support** in the Arduino Boards Manager (`esp32` by Espressif Systems).
2. Open `BioTracker.ino`.
3. Open Library Manager (`Ctrl + Shift + I`) and install:
   * `Adafruit MPU6050`
   * `Adafruit BMP280 Library`
   * `DHT sensor library`
   * `Adafruit Unified Sensor`
4. Select Board: **ESP32 Dev Module** (or **DOIT ESP32 DEVKIT V1**).
5. Set Upload Speed to `921600` and Flash Frequency to `80MHz`.
6. Click **Upload** and open the Serial Monitor at `115200` baud.

---

## 7. Field Operation Instructions
1. Power up the ESP32 using a 3.7V LiPo or portable battery pack.
2. Fasten Pod A (processing pod) to your running shoes or calf band.
3. Fasten Pod B to the sock elastic cuff, ensuring the Y1708 sensor is firmly pressed against the inner ankle bone and the MPU6050 sits on the outer ankle.
4. On your mobile phone, go to Wi-Fi settings and connect to `ESP32_BioTracker` (Password: `biotracker123`).
5. Open your web browser and navigate to `http://192.168.4.1`.
6. Set your body weight and height under **Athlete Profile** and tap **Push to NVS Memory**.
7. Begin your outdoor session!
