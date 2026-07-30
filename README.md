# BioSense-Edge: Precision Bioprocess Environment & Telemetry Monitoring
Author : VIJAY GHANESH G J

---

## 1. Problem Statement

### Problem Being Addressed
In biopharmaceutical manufacturing and cell culture fermentation, biological systems are hyper-sensitive to micro-environmental changes. Unintended fluctuations in pH, Dissolved Oxygen (DO), temperature, or cell density (Optical Density) can lead to cell death, product degradation, or total batch loss. Existing legacy bioreactors often rely on isolated local controllers or expensive proprietary supervisory systems that lack modularity, edge-level fail-safes, and real-time remote telemetry.

### Target Users
* Bioprocess Engineers & Lab Technicians
* Biopharmaceutical R&D Facilities & Academic Bio-Labs
* Industrial Fermentation & Cell-Culture Plant Operators

### Motivation
A single failed 50L bioreactor batch can result in thousands of dollars in wasted media, reagents, and lost throughput. By deploying a low-latency, fault-tolerant IoT monitoring and edge-actuation platform, operators can ensure strict process parameter compliance, receive early anomaly alerts, and maintain full data traceability required for bioprocess validation.

---

## 2. Proposed Solution

### Overview
**BioSense-Edge** is a modular, high-reliability IoT bioprocess telemetry system. It combines precision analog sensing (pH, DO, Temperature, Optical Density) with a high-performance microcontroller node for local processing and PID actuation. Telemetry data is broadcast over a fault-tolerant hybrid network (Wi-Fi/MQTT for high bandwidth; LoRa for long-range/backup) to a central cloud dashboard for real-time analytics and visualization.

### Objectives
1. **Precision Telemetry:** Continuously monitor pH, DO, Temperature, and Optical Density with minimal signal noise and low latency.
2. **Deterministic Edge Control:** Run local PID feedback loops (e.g., closed-loop heating/cooling and peristaltic acid/base dosing) independently of network availability.
3. **Multi-Protocol Communication:** Utilize MQTT over Wi-Fi as the primary channel with automatic fallback to LoRa during network disruption.
4. **Data Integrity & Traceability:** Ensure continuous local SD storage logging (black-box recorder) to guarantee zero data loss during cloud outages.

### Expected Outcome
* Complete prevention of batch contamination/loss due to runaway parameter drift.
* Sub-second local response time to critical bioprocess boundary violations.
* Comprehensive historical parameter logging for process validation and quality control.

---

## 3. System Architecture

### 3.1 High-Level Architecture - refer block diagram 
### 3.2 Data Flow Logic
1. **Acquisition:** Sensors stream raw analog signals -> AFE isolation modules convert signals to calibrated digital values over I2C/SPI.
2. **Edge Execution:** MCU checks values against safe operational thresholds -> Executes local PID algorithm -> Adjusts PWM outputs for heating/dosing pumps.
3. **Local Storage:** Raw and processed logs are written to onboard SD card in CSV format every 1000 ms.
4. **Cloud Telemetry:** Payload packaged into lightweight JSON -> Published to MQTT broker (`biosense/node01/telemetry`) -> Rendered on dashboard.

---

## 4. Hardware & Software Specifications

### Hardware Components
* **Edge MCU:** ESP32-WROOM-32U (Dual-core 240 MHz, Wi-Fi/BLE, integrated SPI/I2C).
* **Galvanic Signal Isolation:** ISO1540 I2C Isolator (prevents ground loop interference between probes in liquid).
* **pH & Dissolved Oxygen AFEs:** Atlas Scientific EZO-pH and EZO-DO embedded circuits.
* **Temperature Sensor:** PT100 Class A RTD with MAX31865 SPI Amplifier.
* **Optical Density (OD600) Sensor:** 600 nm LED source with TSL235R light-to-frequency converter.
* **Backup Comms Module:** Semtech SX1276 LoRa Transceiver module.
* **Actuation Hardware:** 4-Channel Optocoupled Solid State Relay (SSR) module + 12V DC Peristaltic Dosing Pumps.

### Software & Cloud Frameworks
* **Firmware:** C++ / Arduino Framework on VS Code (PlatformIO).
* **Local Protocol:** MQTT (Mosquitto Broker) with TLS encryption.
* **Dashboard / Visualization:** ThingsBoard CE / Node-RED UI.
* **Time-Series Database:** InfluxDB for continuous parameter logging.

---

## 5. Working Methodology

1. System Initialization:** On power-up, the controller initializes hardware buses (I2C, SPI), mounts the local SD card system, and connects to the primary Wi-Fi AP.
2. Sensor Calibration & Sampling:** Sensors undergo a multi-point digital calibration. The MCU polls pH, DO, Temperature, and OD every 500 ms.
3. Edge PID Control Loop:
   * Temperature:** If T_measured < T_setpoint, PWM duty cycle to the heating jacket is increased via PID calculation.
   * pH Adjustment:** If pH > pH_upper_limit, Acid Pump relay triggers for a calculated pulse duration.
     
4. Data Transmission & Fail-Safe Routing:
   * Under normal conditions, data publishes to MQTT every 1 second.
   * If Wi-Fi fails, the network monitor redirects packet broadcast to the SX1276 LoRa module for minimal packet alerting, while storing full-rate logs locally on the SD card.
5.  Due to the presence of dual microcontroller it ensures failsafe. 

---

## 6. Bill of Materials (BOM) & Cost Estimation

Component Description and Quantity 

ESP32 CO-PRocessor  + STM32H7 series main MCU ,2 ,₹420 + ₹2k(STM 32 cost 
Lab Grade pH Probe + Sensor Kit,1, ₹6,250
Dissolved Oxygen Probe + Sensor Kit,1,₹7,500
PT100 Temp Sensor + Driver,1,1,000
OD600 Photodiode Sensor Module,1,₹670
Signal Galvanic Isolators,2 ₹580
SX1276 LoRa Transceiver,1,₹500,₹500
4-Channel Relay Module,1,₹630
12V Peristaltic Dosing Pumps,2,₹750 
SD Logging Shield + 16GB Card,1,₹670
Custom PCB + Casing Enclosure,1,₹1,280
Total Hardware Cost - ₹23,000
---

## 7. Feasibility, Scalability & Future Scope

* **Cost-Efficiency:** Traditional industrial bioprocess controllers cost $3,000 - $10,000. BioSense-Edge offers equivalent core monitoring and edge actuation capability for under $260.
* **Scalability:** Uses a lightweight pub/sub architecture (MQTT). A single broker can manage hundreds of bioreactor nodes simultaneously without bandwidth bottlenecking.
* **Reliability:** Built with strict isolation barrier design to prevent cross-channel liquid interference, coupled with dual-network redundancy (Wi-Fi + LoRa) and local SD fail-safe recording.
* **Future Scope:** Integration of Edge-AI models (TensorFlow Lite for Microcontrollers) to predict cell growth curves (logistic growth modeling) and dynamically adjust nutrient feed rates automatically.
