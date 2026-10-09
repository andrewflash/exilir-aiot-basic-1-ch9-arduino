/* Chapter 9 - OTA dasar ESP32-C5. Upload pertama lewat USB.
 * Isi SSID, password Wi-Fi dan password OTA. Laptop + board satu LAN.
 * Ubah FIRMWARE_VERSION lalu upload melalui network port Arduino IDE.
 * Tetap sertakan ArduinoOTA pada firmware berikutnya.
 */
#include <WiFi.h>
#include <ArduinoOTA.h>

const char *WIFI_SSID = "";          // isi SSID Wi-Fi / hotspot
const char *WIFI_PASSWORD = "";
const char *OTA_PASSWORD = "";       // isi password OTA, berbeda dari Wi-Fi
const char *FIRMWARE_VERSION = "9.0.0";
bool otaReady = false;
uint32_t lastLog = 0;

void setup() {
  Serial.begin(115200);
  // Batas tunggu Serial; board tetap boot tanpa laptop.
  const uint32_t started = millis();
  while (!Serial && millis() - started < 1500) delay(10);
  Serial.printf("Firmware %s\n", FIRMWARE_VERSION);
  if (!WIFI_SSID[0] || !OTA_PASSWORD[0]) {
    Serial.println("Isi WIFI_SSID dan OTA_PASSWORD, lalu upload USB.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ArduinoOTA.setHostname("exilir-ch9-ota");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println("OTA mulai; jaga daya tetap menyala."); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA selesai; board akan reboot."); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA ERROR=%u (0=auth, 1=begin, 2=connect, 3=receive, 4=end)\n", error);
  });
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaReady) {
      ArduinoOTA.begin();
      otaReady = true;
      Serial.print("OTA siap: exilir-ch9-ota, IP=");
      Serial.println(WiFi.localIP());
    }
    ArduinoOTA.handle();
  } else if (otaReady) {
    ArduinoOTA.end();
    otaReady = false; // begin lagi setelah tersambung kembali
  }
  if (millis() - lastLog >= 3000) {
    lastLog = millis();
    Serial.printf("versi=%s uptime=%lu s wifi=%s\n", FIRMWARE_VERSION,
                  (unsigned long)(millis() / 1000),
                  WiFi.status() == WL_CONNECTED ? "connected" : "offline");
  }
  delay(1);
}
