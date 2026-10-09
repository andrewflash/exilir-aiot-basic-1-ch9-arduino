#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>

// Snapshot disalin melalui queue; worker tidak membaca state detector/model
// yang sedang diperbarui loop. Hanya worker ini mengakses objek MQTT.
struct Telemetry {
  uint32_t sequence, inferenceSequence, session, uptimeMs, capturedAt;
  uint32_t steps, modelAgeMs, dspMs, nnMs, gaps, sensorErrors, droppedWindows;
  int modelError;
  bool hasResult, calibrated, calibrationFailed, sensorReady, modelReady;
  float confidence, scores[EI_CLASSIFIER_LABEL_COUNT];
  char activity[12], rawLabel[12];
};

QueueHandle_t telemetryQueue = nullptr;
char deviceId[32], telemetryTopic[128], bootId[12];

void mqttTask(void *) {
  WiFiClient transport;
  transport.setConnectionTimeout(2000); // TCP connect; DNS dapat lebih lama
  PubSubClient mqtt(transport);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setSocketTimeout(2);
  mqtt.setKeepAlive(15);
  if (!mqtt.setBufferSize(1536)) {
    Serial.println("# ERROR: alokasi buffer MQTT gagal");
    vTaskDelete(nullptr);
    return;
  }
  uint32_t lastAttempt = millis() - MQTT_RETRY_MS;
  Telemetry latest = {};
  bool haveLatest = false;
  char payload[1200];
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      if (mqtt.connected()) mqtt.disconnect();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!mqtt.connected() && millis() - lastAttempt >= MQTT_RETRY_MS) {
      lastAttempt = millis();
      // Client ID unik per board; dashboard memiliki client ID berbeda.
      if (mqtt.connect(deviceId)) Serial.printf("# MQTT tersambung: %s\n", telemetryTopic);
      else Serial.printf("# MQTT belum tersambung, state=%d; retry 5 s\n", mqtt.state());
    }
    if (mqtt.connected()) {
      mqtt.loop();
      if (xQueueReceive(telemetryQueue, &latest, 0) == pdTRUE) haveLatest = true;
      // Jangan kirim snapshot lama setelah jaringan lama terputus.
      if (haveLatest && uint32_t(millis() - latest.capturedAt) <= 2500) {
        JsonDocument doc; // ArduinoJson 7
        doc["schema"] = 1;
        doc["device_id"] = deviceId;
        doc["boot_id"] = bootId;
        doc["seq"] = latest.sequence;
        doc["session"] = latest.session;
        doc["inference_seq"] = latest.inferenceSequence;
        doc["uptime_ms"] = latest.uptimeMs;
        doc["steps"] = latest.steps;
        doc["activity"] = latest.activity;
        doc["raw_label"] = latest.rawLabel;
        doc["confidence"] = latest.confidence;
        JsonObject scores = doc["scores"].to<JsonObject>();
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
          scores[ei_classifier_inferencing_categories[i]] = latest.scores[i];
        doc["inference_ms"] = latest.dspMs + latest.nnMs;
        doc["dsp_ms"] = latest.dspMs;
        doc["nn_ms"] = latest.nnMs;
        if (latest.hasResult) doc["model_age_ms"] = latest.modelAgeMs + uint32_t(millis() - latest.capturedAt);
        else doc["model_age_ms"] = nullptr;
        doc["model_error"] = latest.modelError;
        const char *state = "stable";
        if (!latest.sensorReady) state = "sensor_error";
        else if (!latest.modelReady || latest.modelError) state = "model_error";
        else if (latest.calibrationFailed) state = "calibration_failed";
        else if (!latest.calibrated) state = "calibrating";
        else if (!latest.hasResult) state = "waiting";
        else if (latest.modelAgeMs + uint32_t(millis() - latest.capturedAt) > RESULT_TIMEOUT_MS) state = "stale";
        else if (!strcmp(latest.activity, "unknown")) state = "unknown";
        doc["state"] = state;
        doc["sample_gaps"] = latest.gaps;
        doc["sensor_errors"] = latest.sensorErrors;
        doc["dropped_windows"] = latest.droppedWindows;
        doc["firmware"] = FIRMWARE_VERSION;
        // QoS 0, retain=false: dashboard menunggu pesan baru.
        if (measureJson(doc) < sizeof(payload)) {
          const size_t length = serializeJson(doc, payload, sizeof(payload));
          if (mqtt.publish(telemetryTopic, reinterpret_cast<const uint8_t *>(payload), length, false))
            haveLatest = false;
        } else {
          haveLatest = false;
          Serial.println("# ERROR: payload MQTT terlalu besar");
        }
      } else if (haveLatest) haveLatest = false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

bool beginMqttPublisher() {
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceId, sizeof(deviceId), "exilir-c5-%012llX", (unsigned long long)mac);
  snprintf(bootId, sizeof(bootId), "%08lX", (unsigned long)esp_random());
  snprintf(telemetryTopic, sizeof(telemetryTopic), "%s/%s/telemetry", MQTT_TOPIC_ROOT, deviceId);
  Serial.printf("# DEVICE_ID=%s\n# TOPIC=%s\n", deviceId, telemetryTopic);
  if (!WIFI_SSID[0]) {
    Serial.println("# Isi WIFI_SSID dan WIFI_PASSWORD di Config.h untuk MQTT.");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  telemetryQueue = xQueueCreate(1, sizeof(Telemetry));
  if (!telemetryQueue) return false;
  // Connect/reconnect tidak dijalankan dalam loop pembacaan sensor.
  return xTaskCreate(mqttTask, "ch9-mqtt", 8192, nullptr, 0, nullptr) == pdPASS;
}
