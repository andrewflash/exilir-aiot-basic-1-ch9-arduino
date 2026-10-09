#pragma once

// Isi pada laptop peserta. Jangan menyalin kredensial ke MQTT/dashboard.
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// Broker publik tanpa akun untuk latihan. Browser memakai WSS pada port 8084.
const char *MQTT_HOST = "broker.emqx.io";
constexpr uint16_t MQTT_PORT = 1883;
const char *MQTT_TOPIC_ROOT = "exilir/aiot/ch9";
constexpr uint32_t MQTT_RETRY_MS = 5000;
constexpr uint32_t TELEMETRY_MS = 1000;
const char *FIRMWARE_VERSION = "9.1.0-mqtt";

// Sama dengan wiring Chapter 8 dan 9.
constexpr uint8_t SDA_PIN = 2, SCL_PIN = 3, MPU_ADDR = 0x68;
constexpr uint8_t RGB_PIN = 27, BUZZER_PIN = 6;
constexpr bool BUZZER_ACTIVE = false; // HW-508 pasif; KY-012 aktif = true
constexpr float CONFIDENCE_THRESHOLD = 0.80f;
constexpr uint32_t BEEP_MS = 150, COOLDOWN_MS = 1000;
constexpr uint32_t RESULT_TIMEOUT_MS = 7000;
