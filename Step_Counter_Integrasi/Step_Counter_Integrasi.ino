/* Chapter 9: MPU6050 -> langkah + TinyML -> Serial / RGB / buzzer.
 * Board: DFRobot FireBeetle 2 ESP32-C5, ESP32 core 3.x.
 * Simpan Config.h, StepDetector.h, ModelWorker.h di folder sketch ini.
 * Serial 115200: r=reset + kalibrasi; p=Plotter; s=status.
 * OLED 128x32 dan ArduinoOTA opsional; konfigurasi di Config.h.
 */
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"
#include "StepDetector.h"
#include "ModelWorker.h"
#if USE_OLED
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 oled(128, 32, &Wire, -1, 400000, 400000);
bool oledReady = false;
#endif
#if USE_OTA
#include <WiFi.h>
#include <ArduinoOTA.h>
bool otaReady = false, otaUpdating = false;
#endif

constexpr uint32_t SAMPLE_US = 20000;
Adafruit_MPU6050 mpu;
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);
StepDetector detector;
ModelWindow inputWindow = {};
size_t sampleCount = 0;
uint32_t epoch = 0, nextSampleUs = 0, sessionStartMs = 0;
uint32_t missedSlots = 0, sensorErrors = 0, windowsDropped = 0;
bool sensorReady = false, modelReady = false, plotMode = false, calibratedNotice = false;
int votes[3] = {-1, -1, -1}, stableClass = -1, lastBest = -1;
uint8_t voteCount = 0, votePosition = 0;
float lastConfidence = 0;
uint32_t lastResultMs = 0, dspMs = 0, nnMs = 0, lastLogMs = 0, lastDisplayMs = 0;
bool beeping = false, hasBeeped = false;
uint32_t beepStartedMs = 0;

void buzzer(bool on) {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
  else ledcWriteTone(BUZZER_PIN, on ? 2000 : 0);
}

const char *activityName() {
  return stableClass < 0 ? "unknown" : ei_classifier_inferencing_categories[stableClass];
}

void showActivity(int index) {
  stableClass = index;
  const char *label = activityName();
  uint32_t color = rgb.Color(12, 12, 0);
  if (!strcmp(label, "diam")) color = rgb.Color(0, 0, 60);
  else if (!strcmp(label, "jalan")) color = rgb.Color(0, 60, 0);
  else if (!strcmp(label, "lari")) color = rgb.Color(60, 0, 0);
  rgb.fill(color);
  rgb.show();
  if (strcmp(label, "lari")) { buzzer(false); beeping = false; }
}

void clearVotes() {
  for (int &v : votes) v = -1;
  voteCount = votePosition = 0;
  showActivity(-1);
}

void invalidateWindow() {
  ++epoch; // hasil inference dari sebelum gap/reset tidak boleh digunakan
  sampleCount = 0;
  if (modelReady) { xQueueReset(windowQueue); xQueueReset(resultQueue); }
  clearVotes();
}

void resetSession() {
  detector.reset();
  calibratedNotice = false;
  missedSlots = sensorErrors = windowsDropped = 0;
  lastBest = -1;
  lastConfidence = 0;
  invalidateWindow();
  sessionStartMs = millis();
  nextSampleUs = micros() + SAMPLE_US;
  Serial.println("# RESET: diam 3 detik untuk kalibrasi; jumlah mulai dari 0.");
}

void updateBuzzer() {
  if (beeping && millis() - beepStartedMs >= BEEP_MS) {
    buzzer(false);
    beeping = false;
  }
}

void readModelResult() {
  if (!modelReady) return;
  ModelResult result;
  if (xQueueReceive(resultQueue, &result, 0) != pdTRUE) {
    if (stableClass >= 0 && millis() - lastResultMs > RESULT_TIMEOUT_MS) clearVotes();
    return;
  }
  if (result.epoch != epoch) return;
  lastResultMs = millis();
  lastBest = result.best;
  lastConfidence = result.confidence;
  dspMs = result.dspMs;
  nnMs = result.nnMs;
  if (result.error || !isfinite(result.confidence) || result.confidence <= CONFIDENCE_THRESHOLD) {
    clearVotes();
    return;
  }
  votes[votePosition] = result.best;
  votePosition = (votePosition + 1) % 3;
  if (voteCount < 3) ++voteCount;
  int majority = -1;
  if (voteCount == 3) {
    for (int candidate : votes) {
      int count = 0;
      for (int v : votes) if (v == candidate) ++count;
      if (count >= 2) majority = candidate;
    }
  }
  showActivity(majority);
  // Beep hanya untuk lari stabil; tidak per langkah. Cooldown setelah beep.
  if (!strcmp(activityName(), "lari") && !beeping &&
      (!hasBeeped || millis() - beepStartedMs >= BEEP_MS + COOLDOWN_MS)) {
    buzzer(true);
    beeping = hasBeeped = true;
    beepStartedMs = millis();
  }
}

void sampleSensor() {
  if (!sensorReady) return;
  const uint32_t nowUs = micros();
  if (int32_t(nowUs - nextSampleUs) < 0) return;
  const uint32_t late = nowUs - nextSampleUs;
  if (late >= SAMPLE_US) {
    missedSlots += late / SAMPLE_US;
    detector.discardHistory();
    invalidateWindow();
    nextSampleUs = nowUs; // baca satu sampel aktual, jangan catch-up beruntun
  }
  nextSampleUs += SAMPLE_US;
  sensors_event_t a, g, temp;
  if (!mpu.getEvent(&a, &g, &temp) || !isfinite(a.acceleration.x) ||
      !isfinite(a.acceleration.y) || !isfinite(a.acceleration.z)) {
    ++sensorErrors;
    detector.discardHistory();
    invalidateWindow();
    return;
  }
  const uint32_t t = millis() - sessionStartMs;
  detector.update(t, a.acceleration.x, a.acceleration.y, a.acceleration.z);
  if (detector.ready && !calibratedNotice) {
    calibratedNotice = true;
    if (!plotMode) Serial.printf("# MULAI bergerak; baseline=%.3f m/s2\n", detector.baseline);
  }
  if (plotMode) {
    Serial.printf("filtered:%.3f high:%.2f low:%.2f steps:%lu\n", detector.filtered,
                  StepDetector::THRESHOLD_HIGH, StepDetector::THRESHOLD_LOW,
                  (unsigned long)detector.count);
  }
  if (!detector.ready || !modelReady) return;
  const size_t offset = sampleCount * 3;
  // Model menerima XYZ mentah m/s2 termasuk gravitasi, bukan magnitude.
  inputWindow.values[offset] = a.acceleration.x;
  inputWindow.values[offset + 1] = a.acceleration.y;
  inputWindow.values[offset + 2] = a.acceleration.z;
  if (++sampleCount == EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
    inputWindow.epoch = epoch;
    if (xQueueSend(windowQueue, &inputWindow, 0) != pdTRUE) ++windowsDropped;
    sampleCount = 0;
  }
}

void printStatus() {
  const char *raw = lastBest < 0 ? "unknown" : ei_classifier_inferencing_categories[lastBest];
  Serial.printf("t=%lu steps=%lu aktivitas=%s raw=%s conf=%.3f cal=%s gap=%lu sensor_err=%lu ml_drop=%lu DSP=%lu NN=%lu versi=%s\n",
                (unsigned long)(millis() - sessionStartMs), (unsigned long)detector.count,
                activityName(), raw, lastConfidence,
                detector.calibrationFailed ? "failed" : detector.ready ? "ready" : "wait",
                (unsigned long)missedSlots, (unsigned long)sensorErrors,
                (unsigned long)windowsDropped, (unsigned long)dspMs, (unsigned long)nnMs,
                FIRMWARE_VERSION);
}

void updateDisplay() {
#if USE_OLED
  // 128x32 = 512 byte, ~12 ms pada 400 kHz. Sisakan >=16 ms sebelum sampel.
  // Jika modul lambat, gap tetap dilaporkan; tidak ada klaim timing sempurna.
  if (!oledReady || millis() - lastDisplayMs < 1000 || int32_t(nextSampleUs - micros()) < 16000) return;
  lastDisplayMs = millis();
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(activityName());
  oled.print(detector.ready ? "  siap" : "  kalibrasi");
  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.print(detector.count);
  oled.display();
#endif
}

void setupOTA() {
#if USE_OTA
  if (!WIFI_SSID[0] || !OTA_PASSWORD[0]) {
    Serial.println("# Offline: isi SSID dan OTA_PASSWORD di Config.h untuk OTA.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    otaUpdating = true;
    buzzer(false);
    beeping = false;
    if (modelTask) vTaskSuspend(modelTask);
    rgb.fill(rgb.Color(60, 0, 60));
    rgb.show();
    Serial.printf("# OTA mulai; counter dijeda di %lu. Firmware baru akan reset sesi.\n", (unsigned long)detector.count);
  });
  ArduinoOTA.onEnd([]() { Serial.println("# OTA selesai; reboot."); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("# OTA ERROR=%u; sesi dimulai ulang.\n", error);
    otaUpdating = false;
    resetSession();
    if (modelTask) vTaskResume(modelTask);
  });
#endif
}

void handleOTA() {
#if USE_OTA
  if (!WIFI_SSID[0] || !OTA_PASSWORD[0]) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaReady) {
      ArduinoOTA.begin();
      otaReady = true;
      Serial.print("# OTA siap: "); Serial.print(OTA_HOSTNAME);
      Serial.print(" IP="); Serial.println(WiFi.localIP());
    }
    ArduinoOTA.handle(); // layanan update; transfer firmware memang menjeda loop
  } else if (otaReady) {
    ArduinoOTA.end();
    otaReady = false;
  }
#endif
}

void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && millis() - started < 1500) delay(10);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  rgb.begin();
  rgb.setBrightness(40);
  if (!BUZZER_ACTIVE && !ledcAttach(BUZZER_PIN, 2000, 8)) {
    Serial.println("# ERROR: PWM buzzer gagal");
    while (true) delay(100);
  }
  buzzer(false);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(20);
  sensorReady = mpu.begin(MPU_ADDR, &Wire);
  if (sensorReady) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);
    delay(100); // settling hanya saat setup
  } else Serial.println("# ERROR: MPU6050 tidak ditemukan; cek SDA2/SCL3, 0x68.");
#if USE_OLED
  // periphBegin=false: jangan ubah pin Wire yang sudah dikonfigurasi.
  oledReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false);
  if (!oledReady) Serial.println("# OLED gagal; lanjut dengan Serial.");
#endif
  modelReady = sensorReady && beginModelWorker();
  if (sensorReady && !modelReady) Serial.println("# ERROR: task/queue model gagal; counter tetap aktif.");
  setupOTA();
  resetSession();
  if (!sensorReady || !modelReady) { rgb.fill(rgb.Color(60, 0, 60)); rgb.show(); }
  Serial.printf("# Firmware=%s, 50 Hz, r=reset p=Plotter s=status\n", FIRMWARE_VERSION);
}

void loop() {
  updateBuzzer();
  sampleSensor();
  // Batasi jumlah perintah per loop agar sampling tidak kelaparan.
  for (int i = 0; i < 8 && Serial.available(); ++i) {
    const char c = Serial.read();
    if (c == 'r' || c == 'R') resetSession();
    if (c == 'p' || c == 'P') plotMode = !plotMode;
    if ((c == 's' || c == 'S') && !plotMode) printStatus();
  }
  readModelResult();
  handleOTA();
#if USE_OTA
  if (otaUpdating) { delay(1); return; }
#endif
  if (!plotMode && millis() - lastLogMs >= 1000) {
    lastLogMs = millis();
    printStatus();
  }
  updateDisplay();
  // Memberi task inference prioritas 0 waktu CPU; tidak memakai delay panjang.
  delay(1);
}
