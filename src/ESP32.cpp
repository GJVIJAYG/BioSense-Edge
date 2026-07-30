#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// --- HARDWARE PIN DEFINITIONS (STM32H7 Main Controller) ---
#define HEATER_SSR_PIN    D1
#define ACID_PUMP_PIN     D32
#define BASE_PUMP_PIN     D5
#define SD_CS_PIN         D4

// --- TARGET BIOPROCESS PARAMETERS ---
const float TEMP_SETPOINT  = 37.0; // Target temp in Celsius
const float PH_LOWER_LIMIT = 6.8;  // Lower pH threshold
const float PH_UPPER_LIMIT = 7.4;  // Upper pH threshold

// --- FUNCTION DECLARATIONS ---
float readTemperatureRTD();
float readPHProbe();
float readDissolvedOxygen();
float readOpticalDensity();
void executeClosedLoopPID(float currentTemp, float currentPH);
void logDataToIndustrialSD(float temp, float ph, float doVal, float odVal);
void transmitToESP32CoProcessor(float temp, float ph, float doVal, float odVal);

void setup() {
  Serial.begin(115200);   // Debug UART
  Serial1.begin(115200);  // Inter-Processor Communication (UART to ESP32)
  
  pinMode(HEATER_SSR_PIN, OUTPUT);
  pinMode(ACID_PUMP_PIN, OUTPUT);
  pinMode(BASE_PUMP_PIN, OUTPUT);
  
  // Initialize Industrial SD Logging Module
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[ERROR] SD Black-Box recorder initialization failed!");
  } else {
    Serial.println("[SYSTEM] SD Logger initialized. Fail-safe active.");
  }

  Serial.println("[SYSTEM] BioSense-Edge STM32H7 Core Online.");
}

void loop() {
  // 1. Precision Analog Sampling (STM32H7 ADC/SPI)
  float temp  = readTemperatureRTD();
  float ph    = readPHProbe();
  float doVal = readDissolvedOxygen();
  float odVal = readOpticalDensity();

  // 2. Execute Real-Time Edge PID Control
  executeClosedLoopPID(temp, ph);

  // 3. Fail-Safe Local SD Logging
  logDataToIndustrialSD(temp, ph, doVal, odVal);

  // 4. Offload Telemetry Payload to ESP32 Co-Processor (UART Stream)
  transmitToESP32CoProcessor(temp, ph, doVal, odVal);

  delay(500); // High-frequency 2 Hz control loop execution
}

float readTemperatureRTD() {
  // Simulated MAX31865 RTD SPI Read
  return 36.95 + ((rand() % 10) / 100.0);
}

float readPHProbe() {
  // Simulated Isolated I2C Read from EZO-pH
  return 7.15 + ((rand() % 10) / 100.0);
}

float readDissolvedOxygen() {
  // Simulated Isolated I2C Read from EZO-DO (%)
  return 98.2 + ((rand() % 5) / 10.0);
}

float readOpticalDensity() {
  // Simulated OD600 Absorbance Reading
  return 0.45 + ((rand() % 2) / 100.0);
}

void executeClosedLoopPID(float currentTemp, float currentPH) {
  // Thermal Relay Control Logic
  if (currentTemp < TEMP_SETPOINT) {
    digitalWrite(HEATER_SSR_PIN, HIGH);
  } else {
    digitalWrite(HEATER_SSR_PIN, LOW);
  }

  // pH Automated Acid/Base Dosing Logic
  if (currentPH > PH_UPPER_LIMIT) {
    digitalWrite(ACID_PUMP_PIN, HIGH);
    delay(200); // 200ms pulsed dosing
    digitalWrite(ACID_PUMP_PIN, LOW);
  } else if (currentPH < PH_LOWER_LIMIT) {
    digitalWrite(BASE_PUMP_PIN, HIGH);
    delay(200);
    digitalWrite(BASE_PUMP_PIN, LOW);
  }
}

void logDataToIndustrialSD(float temp, float ph, float doVal, float odVal) {
  File logFile = SD.open("/biolog.csv", FILE_WRITE);
  if (logFile) {
    logFile.print(millis());
    logFile.print(",");
    logFile.print(temp);
    logFile.print(",");
    logFile.print(ph);
    logFile.print(",");
    logFile.print(doVal);
    logFile.print(",");
    logFile.println(odVal);
    logFile.close();
  }
}

void transmitToESP32CoProcessor(float temp, float ph, float doVal, float odVal) {
  // Send formatted JSON over Serial1 to ESP32 for MQTT Cloud Sync
  String jsonPayload = "{\"temp\":" + String(temp) + 
                       ",\"ph\":" + String(ph) + 
                       ",\"do\":" + String(doVal) + 
                       ",\"od\":" + String(odVal) + "}";
  Serial1.println(jsonPayload);
}
