#pragma once
#include <stdint.h>
#include <math.h>
#include <algorithm>

// Diadaptasi dari Chapter_4/Slide/lab/Exilir_Step_Counter/StepDetector.h.
// m/s2; waktu relatif sesi; 50 Hz. ML tidak menambah jumlah langkah.
class StepDetector {
 public:
  static constexpr float THRESHOLD_HIGH = 1.2f;
  static constexpr float THRESHOLD_LOW = 0.3f;
  static constexpr uint32_t MIN_INTERVAL_MS = 300;
  static constexpr uint32_t BASELINE_MS = 3000;
  uint32_t count = 0, gaps = 0;
  float baseline = 0, filtered = 0;
  bool ready = false, calibrationFailed = false;

  void reset() { *this = StepDetector(); }

  // Panggil jika sensor/timing gagal; jumlah dan baseline tetap disimpan.
  void discardHistory() {
    history = windowCount = windowPos = 0;
    sum = 0;
    armed = false; // harus turun < threshold bawah sebelum menerima puncak
  }

  bool update(uint32_t t_ms, float ax, float ay, float az) {
    const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
    if (!isfinite(magnitude)) { discardHistory(); return false; }
    if (seen && uint32_t(t_ms - lastSample) > 60) { ++gaps; discardHistory(); }
    seen = true;
    lastSample = t_ms;
    if (t_ms < BASELINE_MS && calibrationCount < 150)
      calibration[calibrationCount++] = magnitude;
    if (!ready && !calibrationFailed && t_ms >= BASELINE_MS) {
      if (calibrationCount < 30) calibrationFailed = true;
      else {
        std::sort(calibration, calibration + calibrationCount);
        const unsigned mid = calibrationCount / 2;
        baseline = calibrationCount % 2 ? calibration[mid] : (calibration[mid - 1] + calibration[mid]) / 2;
        ready = true;
      }
    }
    if (windowCount == 5) sum -= window[windowPos];
    else ++windowCount;
    window[windowPos] = magnitude;
    sum += magnitude;
    windowPos = (windowPos + 1) % 5;
    const float smooth = sum / windowCount;
    filtered = ready ? smooth - baseline : 0;
    bool accepted = false;
    if (ready && windowCount == 5 && history >= 2) {
      if (filtered < THRESHOLD_LOW) armed = true;
      const bool peak = previous > older && previous >= smooth && previous - baseline > THRESHOLD_HIGH;
      const bool spaced = !hasPeak || uint32_t(previousTime - lastPeak) >= MIN_INTERVAL_MS;
      if (previousTime >= BASELINE_MS && armed && peak && spaced) {
        ++count;
        lastPeak = previousTime;
        hasPeak = true;
        armed = false;
        accepted = true;
      }
    }
    older = previous;
    previous = smooth;
    previousTime = t_ms;
    if (history < 2) ++history;
    return accepted;
  }

 private:
  float calibration[150] = {}, window[5] = {}, sum = 0, older = 0, previous = 0;
  unsigned calibrationCount = 0, windowCount = 0, windowPos = 0, history = 0;
  uint32_t previousTime = 0, lastPeak = 0, lastSample = 0;
  bool armed = true, hasPeak = false, seen = false;
};
