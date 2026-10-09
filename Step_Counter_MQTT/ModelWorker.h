#pragma once
// Helper integrasi: inference berjalan di task prioritas 0.
// loop Arduino (prioritas 1) tetap membaca sensor setiap 20 ms.
// Ini berbagi CPU ESP32-C5, bukan mengandalkan core kedua.
#include <xflash-project-1_inferencing.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 3, "Model harus XYZ");
static_assert(EI_CLASSIFIER_FREQUENCY == 50, "Model harus 50 Hz");
static_assert(EI_CLASSIFIER_RAW_SAMPLE_COUNT == 100, "Model harus window 2 s");

struct ModelWindow {
  uint32_t epoch;
  float values[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
};
struct ModelResult {
  uint32_t epoch;
  int error, best;
  float confidence;
  float scores[EI_CLASSIFIER_LABEL_COUNT];
  uint32_t dspMs, nnMs;
};

QueueHandle_t windowQueue = nullptr, resultQueue = nullptr;
TaskHandle_t modelTask = nullptr;

void inferenceTask(void *) {
  ModelWindow window;
  while (true) {
    if (xQueueReceive(windowQueue, &window, portMAX_DELAY) != pdTRUE) continue;
    signal_t signal;
    ei_impulse_result_t prediction = {};
    ModelResult out = {};
    out.epoch = window.epoch;
    out.best = -1;
    out.error = numpy::signal_from_buffer(window.values, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (!out.error) out.error = run_classifier(&signal, &prediction, false);
    if (!out.error) {
      out.best = 0;
      for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
        if (prediction.classification[i].value > prediction.classification[out.best].value)
          out.best = i;
      for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
        out.scores[i] = prediction.classification[i].value;
      out.confidence = prediction.classification[out.best].value;
      out.dspMs = prediction.timing.dsp;
      out.nnMs = prediction.timing.classification;
    }
    // Queue panjang 1: hasil terbaru menggantikan hasil yang belum dibaca.
    xQueueOverwrite(resultQueue, &out);
  }
}

bool beginModelWorker() {
  windowQueue = xQueueCreate(1, sizeof(ModelWindow));
  resultQueue = xQueueCreate(1, sizeof(ModelResult));
  if (!windowQueue || !resultQueue) return false;
  // ESP-IDF menghitung ukuran stack task dalam byte.
  return xTaskCreate(inferenceTask, "ch9-inference", 16384, nullptr, 0, &modelTask) == pdPASS;
}
