# A Project Report
### on
# <span style="color:red">Sensors And Intrumentation lab</span>

<br>

#### Submitted for the Partial Fulfillment of the Requirements for the degree of
### **Bachelor of Technology**
*in*
### **Electronics and Communication Engineering**

<br>

*by*

**UG25ECE068 – Shreedhar Joshi**  
**UG25ECE079 – Veer Chavda**  

<br>

### Under the guidance of
**Dr. Venkata A. P. Chavali, Dr. Sudeep Sharma**

<br>

<div align="center">
  <img src="iiit_surat_logo.png" alt="Indian Institute of Information Technology Surat Logo" width="130"/>
</div>

<br>

**ELECTRONICS AND COMMUNICATION ENGINEERING**  
**November, 2026**  
**<span style="color:red">INDIAN INSTITUTE OF INFORMATION TECHNOLOGY SURAT-394190</span>**

---

# Project Report: Off-Grid Biometric Ankle Tracker (BioTracker)
*An IoT-Based Real-Time Kinematic, Biometric, and Microclimate Wearable System*

---

## 1. Executive Summary & Objectives

The Off-Grid Biometric Ankle Tracker (BioTracker) is an Internet of Things (IoT)-based wearable instrumentation system engineered to bypass the inaccuracies and momentum artifacts ("floppy effect") of conventional wrist-based fitness trackers. Utilizing an ESP32 microcontroller paired with inertial measurement, optical photoplethysmography (PPG), barometric altimetry, and localized skin microclimate sensing powered portably via a 5V USB power bank, the system accurately tracks footfall kinematics, cardiovascular pulse, vertical work against gravity, and perspiration dynamics in real-time [1].

The primary objectives of this project are:

* **Lower-Limb Kinematic Pedometer & Cadence:** Direct measurement of tibial ground-impact deceleration vectors to eliminate false positives caused by arm gestures [2].
* **Cardiovascular Pulse Extraction:** Non-invasive optical PPG sensing over the posterior tibial capillary bed, filtered via dynamic baseline tracking.
* **Vertical Work & Incline Caloric Calculation:** Atmospheric pressure compensation using a deadband filter to accurately calculate mechanical work performed during climbing.
* **Footwear Microclimate Hydration Monitoring:** Measurement of enclosed skin relative humidity to compute real-time sweat rates and hydration status tiers [3].
* **Autonomous Off-Grid Dashboard:** A self-contained, zero-dependency local Wi-Fi SoftAP server hosting a real-time AJAX analytics dashboard without cloud or internet reliance.

---

## 2. System Architecture

The system architecture integrates physical wearable hardware sensors with an embedded real-time web server to deliver autonomous, off-grid telemetry.

### 2.1 Hardware Components

| Component | Description and Purpose |
| :--- | :--- |
| **ESP32 Development Board** | The main processing unit executing digital signal conditioning, algorithmic filtering, NVS flash profile management, and serving the local Wi-Fi SoftAP web server [4]. |
| **MPU6050 6-Axis IMU** | Captures 3D linear acceleration and angular velocity vectors at 50 Hz on the lateral malleolus to detect locomotive ground-impact shockwaves. |
| **Y1708 Optical PPG Sensor** | Measures arterial blood volume pulse waveforms using green optical reflection (570 nm) placed over the posterior tibial artery on the medial malleolus. |
| **BMP280 Barometric Altimeter** | Piezoresistive pressure sensor delivering high-precision altitude and vertical ascent data to compute thermodynamic climb energy. |
| **DHT22 / DHT11 Sensor** | Monitors trapped skin microclimate relative humidity (RH) and ambient temperature to quantify localized perspiration rates and hydration tiers. |
| **Portable 5V USB Power Bank** | Supplies regulated 5V DC power directly via the ESP32 Micro-USB port, delivering stable, continuous operation during locomotion without tethering to AC mains. |

### 2.2 Software and Firmware Technologies

* **PlatformIO & Arduino C/C++:** Used for programming bare-metal ESP32 firmware, hardware interrupts, and real-time sensor polling loops [5].
* **Web Technologies (HTML5, CSS3, JavaScript):** Forms the zero-dependency, responsive dark-mode mobile dashboard UI served directly from ESP32 flash.
* **Asynchronous Web Server (ESPAsyncWebServer):** Handles RESTful API data serialization (`/api/data`), parameter updates (`/api/profile`), and session resets.
* **HTML5 Canvas Graphics:** Utilized for real-time visualization of dynamic acceleration magnitudes and perspiration trendlines without external libraries.
* **Non-Volatile Storage (Preferences.h):** Retains user biometric profiles (body mass, height) across power resets for customized calorie calculations.

---

## 3. Circuit Design and Electrical Integration

Proper electrical integration is critical to protect sensitive mixed-signal sensors, minimize noise coupling on analog channels, and prevent I2C bus lockups [6].

### 3.1 MPU6050 & BMP280 Sensor Connections (Shared I2C Bus)

The MPU6050 IMU and BMP280 altimeter communicate over a shared 3.3V high-speed I2C bus operating at 400 kHz.

* **VCC:** Connected to ESP32 3.3V power rail with local decoupling capacitors (100 nF ceramic).
* **GND:** Connected to common star ground (GND).
* **SDA:** Connected to ESP32 GPIO 21 with a 4.7 kΩ pull-up resistor to 3.3V.
* **SCL:** Connected to ESP32 GPIO 22 with a 4.7 kΩ pull-up resistor to 3.3V.

### 3.2 Y1708 Optical PPG Sensor Connections

To eliminate radio-frequency (RF) cross-talk caused by the ESP32 Wi-Fi module, the analog PPG signal is dedicated to ADC1 (which remains completely uncoupled from Wi-Fi hardware locks).

* **VCC:** Connected to ESP32 3.3V pin.
* **GND:** Connected to common ground.
* **SIGNAL:** Connected to ESP32 GPIO 32 (ADC1 Channel 4).

### 3.3 DHT22 Sensor Connections

* **VCC:** Connected to ESP32 3.3V pin.
* **GND:** Connected to common ground.
* **DATA:** Connected to ESP32 GPIO 4 with a 10 kΩ pull-up resistor.

---

## 4. Mathematical Modeling and Kinematic Calculations

The ESP32 processes raw sensor timings and ADC signals into actionable metrics using the following mathematical formulas:

$$\|A\| = \sqrt{a_x^2 + a_y^2 + a_z^2}$$

$$A_{\text{net}} = | \|A\|_{\text{filtered}} - 9.80665 |$$

$$\text{DC}_t = \alpha \cdot \text{DC}_{t-1} + (1 - \alpha) \cdot \text{Raw}_t \quad (\alpha = 0.95)$$

$$\text{BPM} = \frac{60000}{\text{IBI (ms)}}$$

$$\text{Altitude } (h) = 44330.0 \times \left(1.0 - \left(\frac{P}{1013.25}\right)^{\frac{1}{5.255}}\right)$$

$$\text{Sweat Rate} = \frac{\Delta\text{RH}}{\Delta t_{\text{minutes}}} \quad [\%/\text{min}]$$

$$\text{Calories}_{\text{Total}} = \left[ (\text{Steps} \times 0.040) + (\text{Elevation Gain} \times 0.150) \right] \times \left( \frac{\text{Mass}_{\text{kg}}}{70.0} \right)$$

### 4.1 Operational & Algorithm Parameters

| Parameter | Value |
| :--- | :--- |
| **IMU Polling Frequency** | 50 Hz (20 ms interval) |
| **Net Acceleration Footfall Threshold** | 2.50 m/s² above 1g baseline |
| **Pedometer Debounce Lockout Hysteresis** | 350 ms lockout window (< 2.85 steps/sec) |
| **PPG Sampling Rate & Resolution** | 100 Hz (10 ms interval), 12-bit ADC (0–4095) |
| **PPG Physiological Bounds** | 45 BPM to 215 BPM (280 ms peak cooldown) |
| **Altimeter Noise Deadband Filter** | 0.50 m threshold (rejects thermal turbulence) |
| **Perspiration Rolling Derivative Window** | 60 seconds sliding time window |
| **Wi-Fi SoftAP Configuration** | SSID: ESP32_BioTracker (192.168.4.1) |

---

## 5. System Workflow & Data Flow

The operational workflow executes a continuous mixed-signal monitoring cycle. The system initializes hardware buses, mounts flash NVS, and starts the local Wi-Fi SoftAP powered via the portable USB power bank. The ESP32 sequentially polls the IMU at 50 Hz, the PPG sensor at 100 Hz, the altimeter at 1 Hz, and the DHT22 microclimate sensor at 0.5 Hz. Signals pass through the mathematical filters to update cadence, heart rate, elevation, and hydration tiers. Processed metrics are streamed via JSON to the local dashboard.

```
                     ┌──────────────────┐
                     │      START       │
                     └────────┬─────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │       Power On via 5V USB Power Bank      │
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │     Initialize Wi-Fi SoftAP & Flash NVS   │
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │  Read Sensor Suite (MPU6050, Y1708, etc.) │
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │ Execute Biometric & Kinematic Algorithms  │
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │    Update Metrics Payload & Flash Memory  │
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │ Stream Data to Local Dashboard (/api/data)│
        └─────────────────────┬─────────────────────┘
                              │
                              ▼
        ┌───────────────────────────────────────────┐
        │   Render Live Metrics & Canvas Trends     │
        └─────────────────────┬─────────────────────┘
                              │
               ↻ (Repeat Continuous Monitoring Cycle)
```

---

## 6. Custom Web Dashboard

The user interface is an autonomous, dark-mode web dashboard served directly from the ESP32’s flash program memory at `http://192.168.4.1`. Key features include zero-dependency styling (no external CDN dependencies), real-time AJAX streaming updating all telemetry at 1,000 ms intervals, an HTML5 live canvas trendline for acceleration vectors and perspiration curves, two-way NVS profile calibration (allowing athletes to adjust height and mass dynamically), and manual calibration controls (zeroing elevation gain and resetting session step tallies).

---

## 7. Advanced Analytics and Environmental Compensation

The project incorporates dedicated algorithmic compensation techniques to overcome physical environmental and biological noise artifacts.

### 7.1 Data Analytics & Hydration Categorization

The system continuously evaluates the delta relative humidity ($\Delta\text{RH} = \text{RH}_{\text{skin}} - \text{RH}_{\text{ambient}}$) and the 60-second rolling derivative to categorize hydration state into four actionable tiers: *Basal/Dry* (< 5%), *Mild Perspiration* (5–15%), *Active Sweat* (15–30%), and *Heavy Sweat / Hydrate Now* (> 30% or rate > 3.0%/min).

### 7.2 Barometric Deadband & Optical Baseline Tracking

Atmospheric air drafts induce false elevation jitter. A 0.50-meter deadband filter discards minor pressure oscillations, while a monotonic positive-accumulation filter ensures that descent does not subtract from vertical work accomplished:

$$\Delta h_{\text{effective}} = \begin{cases} \Delta h, & \text{if } |\Delta h| \ge 0.50\text{ m} \\ 0.0\text{ m}, & \text{if } |\Delta h| < 0.50\text{ m} \end{cases}$$

$$\text{Elevation Gain} = \sum \max(0, \Delta h_{\text{effective}})$$

Concurrently, the optical PPG sensor dynamically subtracts tissue optical absorption baselines using an EWMA filter ($\alpha = 0.95$), isolating pure pulsatile AC arterial blood swings.

---

## 8. Advantages and Limitations

### 8.1 System Advantages

* **Kinematic Immunity to Upper-Limb Gestures:** Directly captures tibial ground-impact shockwaves (> 2.50 m/s²), completely isolating locomotion from upper extremity movements (typing, gesturing, clapping) and eliminating the classic "floppy effect" [1].
* **Accurate Tracking Under Constrained Arm Postures:** Reliably detects footfalls when arms are immobilized (e.g., pushing carts, holding treadmill handrails, carrying objects) where conventional wristbands register severe false negatives.
* **True Vertical Work & Caloric Accounting:** Combines barometric elevation gain with horizontal footfall kinematics to directly calculate mechanical work against gravity (0.150 kcal/m), outperforming flat-step empirical models.
* **Direct Microclimate Hydration & Perspiration Capture:** Exploits the enclosed footwear boundary layer to monitor local relative humidity buildup and compute a dynamic sweat rate derivative before ambient air evaporation occurs [3].
* **Completely Autonomous Off-Grid Operation:** Hosts a local Wi-Fi SoftAP at `192.168.4.1` with a zero-dependency dark-mode dashboard, operating reliably in remote wilderness without cellular networks, Bluetooth pairing, or cloud infrastructure.

### 8.2 Current Limitations & Challenges

* **Absence of Dedicated Custom Onboard Battery:** The current prototype relies on an external portable 5V USB power bank rather than a custom miniaturized lithium-polymer (LiPo) power cell with an integrated Battery Management System (BMS).
* **Lack of Single-Unit Wearable Packaging:** The microcontroller, sensors, and power wiring are connected as discrete prototyping modules rather than being seamlessly unified into a single ergonomic wearable casing or custom PCB.
* **Ephemeral In-Memory Streaming (No Historical Logging):** Biometric metrics are streamed live in volatile RAM and browser buffers without persistent storage (e.g., flash memory or SD card) for multi-session timeline tracking or retrospective clinical analysis.
* **Analog PPG Susceptibility & Lack of Oximetry ($\text{SpO}_2$):** The single-wavelength optical phototransistor lacks active ambient light cancellation and cannot compute blood oxygen saturation ($\text{SpO}_2$), making it susceptible to micro-decoupling during vigorous sprinting.
* **Sensor Diffusion Latency & Power Bank Sleep Thresholds:** Polymer capacitive humidity sensors exhibit physical moisture diffusion lag (6–15 s), while certain smart power banks require continuous active load current to prevent auto-sleep shutdowns.

---

## 9. Future Scope & Applications

Future development will focus on transitioning the experimental prototype into a unified, commercial-grade wearable ecosystem through several strategic advancements:

* **Seamless Single-Piece Wearable Integration:** Designing a custom 4-layer rigid-flex PCB and an ergonomic, water-resistant silicone ankle enclosure that unifies the microcontroller, sensors, and power source into one single wearable device.
* **Dedicated Miniature Battery with Power Management:** Integrating an onboard high-density rechargeable LiPo pouch cell with an integrated charge management IC (e.g., TP4056 or BQ24075) to completely eliminate reliance on external power banks.
* **Historical Data Logging & Timeline Generation:** Incorporating an onboard SPI flash memory chip (or microSD interface) along with an embedded time-series database (e.g., LittleFS / SQLite) to record continuous biometric sessions for retrospective trend analysis.
* **Replacement of PPG with Dedicated Pulse Oximeter (MAX30102):** Upgrading the analog sensor to a medical-grade optical pulse oximeter module featuring dual-wavelength (660 nm Red / 880 nm Infrared) LEDs, 18-bit delta-sigma ADCs, and digital ambient light cancellation to simultaneously provide accurate heart rate (BPM) and blood oxygen saturation ($\text{SpO}_2$).
* **GNSS / GPS Module Integration (Add-on):** Incorporating an ultra-compact GNSS/GPS receiver (e.g., u-blox NEO-6M or SAM-M8Q) to enable geospatial trajectory tracking, true ground speed calculation, and outdoor route mapping alongside elevation and footstep kinematics.
* **Ultra-Low-Power BLE 5.0 & Edge TinyML:** Migrating to an nRF52840 SoC to achieve > 60 hours of operational battery life, paired with an onboard 1D-CNN classifier for automatic gait mode identification.

---

## 10. Conclusion

The Off-Grid Biometric Ankle Tracker (BioTracker) successfully demonstrates how multi-modal sensor fusion, biomechanical lower-limb placement, and embedded web technologies can overcome the fundamental accuracy flaws of conventional wrist-worn wearables. By capturing true ground-reaction impact vectors, posterior tibial arterial pulses, vertical barometric work, and localized perspiration microclimates powered reliably via a portable USB power bank, the system provides a robust, field-tested foundation for athletic training, military expeditions, and clinical gait rehabilitation.

---

## References

1. Nature Scientific Reports, "Accuracy and reliability of accelerometer-based pedometers: Lower-limb vs. wrist placement," *Sci. Rep.*, vol. 14, p. 39532, 2024.
2. MDPI Sensors, "Frequency Analysis of Human Locomotion Shockwaves and Gait Cadence Characteristics," *Sensors*, vol. 25, no. 18, p. 5657, 2025.
3. MDPI Sensors, "Wearable Localized Microclimate Sensing for Perspiration and Thermal Comfort Monitoring," *Sensors*, vol. 26, no. 17, p. 5640, 2026.
4. Espressif Systems, "ESP-IDF Programming Guide: Wi-Fi SoftAP and ADC Hardware Subsystems," ESP32 Documentation. [Online]. Available: https://docs.espressif.com/projects/esp-idf/
5. InvenSense Inc., "MPU-6000 and MPU-6050 Product Specification and Register Map," Document Revision 3.4, 2021.
6. Bosch Sensortec, "BMP280 Digital Pressure Sensor Technical Data Sheet," Document BST-BMP280-DS001-19, 2022.
