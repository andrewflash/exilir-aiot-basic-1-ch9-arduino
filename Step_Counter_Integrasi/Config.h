#pragma once

// Konfigurasi latihan. Default: Serial + TinyML + feedback, tanpa OLED.
// Nilai dapat juga dioverride oleh build flags saat validasi.
#ifndef USE_OLED
#define USE_OLED 0
#endif
#ifndef USE_OTA
#define USE_OTA 1
#endif

constexpr uint8_t SDA_PIN = 2, SCL_PIN = 3;
constexpr uint8_t MPU_ADDR = 0x68;
constexpr uint8_t RGB_PIN = 27, BUZZER_PIN = 6;
constexpr bool BUZZER_ACTIVE = false; // HW-508 pasif; KY-012 aktif = true
constexpr float CONFIDENCE_THRESHOLD = 0.80f;
constexpr uint32_t BEEP_MS = 150, COOLDOWN_MS = 1000;
constexpr uint32_t RESULT_TIMEOUT_MS = 7000;

// OLED opsional: SSD1306 I2C 128x32, bukan 128x64. Alamat cek pada modul.
constexpr uint8_t OLED_ADDR = 0x3C;

// Kosong = offline; SSID + password OTA wajib diisi untuk mengaktifkan OTA.
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";
const char *OTA_PASSWORD = "";
const char *OTA_HOSTNAME = "exilir-ch9-counter";
const char *FIRMWARE_VERSION = "9.0.0";
