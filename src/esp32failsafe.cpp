/**
  ******************************************************************************
  * @file           : esp32_failsafe.cpp
  * @brief          : ESP32 Dedicated Network Co-Processor & Failsafe Controller
  *                   Handles Wi-Fi Monitoring, MQTT Publishing, and Hardware
  *                   LoRa Fallback Trigger upon Network Disruption.
  ******************************************************************************
  */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* --- NETWORK CONFIGURATION --- */
const char* WIFI_SSID     = "BIO_LAB_5G";
const char* WIFI_PASS     = "SecureLabAccess2026";
const char* MQTT_SERVER   = "192.168.1.150";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "biosense/node01/telemetry";
const char* MQTT_ALERT    = "biosense/node01/alerts";

/* --- PIN DEFINITIONS --- */
#define LORA_ENABLE_PIN   5   // Active HIGH to enable LoRa backup module
#define WATCHDOG_LED_PIN  2   // Onboard LED indicator
#define STM32_UART_RX     16  // Hardware Serial2 RX from STM32H7
#define STM32_UART_TX     17  // Hardware Serial2 TX to STM32H7

/* --- NETWORK & FAILSAFE TIMERS --- */
unsigned long lastPacketRxTime = 0;
unsigned long lastMqttRetry = 0;
const unsigned long PACKET_TIMEOUT_MS = 5000; // 5-second watchdog threshold
const unsigned long MQTT_RETRY_INTERVAL = 3000;

/* --- GLOBAL OBJECTS --- */
WiFiClient espClient;
PubSubClient mqttClient(espClient);

/* --- FUNCTION PROTOTYPES --- */
void ConnectToWiFi();
void ConnectToMQTT();
void ProcessIncomingSTM32Payload(String jsonPayload);
void TriggerLoRaFailover(String alertMessage);

void setup() {
    Serial.begin(115200);                  // Debug Monitor
    Serial2.begin(115200, SERIAL_8N1, STM32_UART_RX, STM32_UART_TX); // Inter-MCU Bus

    pinMode(LORA_ENABLE_PIN, OUTPUT);
    pinMode(WATCHDOG_LED_PIN, OUTPUT);
    digitalWrite(LORA_ENABLE_PIN, LOW);    // Default: Primary Wi-Fi Mode

    Serial.println("[ESP32] Network Co-Processor Booting...");
    ConnectToWiFi();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
    // 1. Maintain Wi-Fi & MQTT State Machine
    if (WiFi.status() != WL_CONNECTED) {
        ConnectToWiFi();
    } else if (!mqttClient.connected()) {
        if (millis() - lastMqttRetry > MQTT_RETRY_INTERVAL) {
            lastMqttRetry = millis();
            ConnectToMQTT();
        }
    } else {
        mqttClient.loop();
    }

    // 2. Read UART Telemetry Stream from STM32H7 Core
    if (Serial2.available() > 0) {
        String payload = Serial2.readStringUntil('\n');
        payload.trim();

        if (payload.length() > 0) {
            lastPacketRxTime = millis(); // Refresh Watchdog Timer
            digitalWrite(WATCHDOG_LED_PIN, !digitalRead(WATCHDOG_LED_PIN)); // Blink heartbeat

            ProcessIncomingSTM32Payload(payload);
        }
    }

    // 3. Communications Watchdog Check
    if (millis() - lastPacketRxTime > PACKET_TIMEOUT_MS && lastPacketRxTime != 0) {
        Serial.println("[FAILSAFE WARNING] Inter-MCU Communication Timeout! Check STM32 Core.");
        TriggerLoRaFailover("{\"alert\":\"STM32_UART_TIMEOUT\"}");
    }
}

/**
  * @brief Non-blocking Wi-Fi Reconnection Logic
  */
void ConnectToWiFi() {
    static unsigned long lastWifiAttempt = 0;
    if (millis() - lastWifiAttempt > 5000) {
        lastWifiAttempt = millis();
        Serial.println("[ESP32] Connecting to Laboratory Wi-Fi Network...");
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
}

/**
  * @brief Non-blocking MQTT Broker Connection Logic
  */
void ConnectToMQTT() {
    Serial.println("[ESP32] Connecting to Centralized MQTT Broker...");
    String clientId = "BioSenseNode-ESP32-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
        Serial.println("[ESP32] MQTT Connected. Disabling Backup Radio.");
        digitalWrite(LORA_ENABLE_PIN, LOW); // Return to primary high-bandwidth path
        mqttClient.publish(MQTT_ALERT, "{\"status\":\"PRIMARY_LINK_RESTORED\"}");
    } else {
        Serial.print("[ESP32] MQTT Connection Failed, rc=");
        Serial.println(mqttClient.state());
    }
}

/**
  * @brief Route Telemetry Data over MQTT or Fallback to LoRa Transceiver
  */
void ProcessIncomingSTM32Payload(String jsonPayload) {
    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
        // Primary Path: High-throughput JSON over Wi-Fi/MQTT
        mqttClient.publish(MQTT_TOPIC, jsonPayload.c_str());
        Serial.println("[MQTT TX] " + jsonPayload);
    } else {
        // Fail-Safe Path: Trigger Backup LoRa Broadcast
        Serial.println("[FAILSAFE ROUTER] Wi-Fi/Cloud down. Falling back to LoRa Transceiver.");
        TriggerLoRaFailover(jsonPayload);
    }
}

/**
  * @brief Activates LoRa Module and Transmits Packet via SPI / Direct Radio Trigger
  */
void TriggerLoRaFailover(String alertMessage) {
    digitalWrite(LORA_ENABLE_PIN, HIGH); // Assert Hardware Enable line to LoRa Radio
    
    // Pulse LoRa Trigger Line or Relay Compact Packet over Sub-GHz Radio
    Serial.print("[LORA FAILOVER TX] ");
    Serial.println(alertMessage);
}
