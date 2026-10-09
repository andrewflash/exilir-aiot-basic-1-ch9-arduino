// Uji sinyal sintetis; tidak mengukur akurasi langkah manusia.
#include "../Step_Counter_Integrasi/StepDetector.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <limits>

static void feed(StepDetector &d, unsigned from, unsigned to, float amplitude, unsigned interval = 600) {
  for (unsigned t = from; t < to; t += 20) {
    float z = 9.81f;
    for (unsigned i = 0; i < 20; ++i) {
      const float distance = (float(t) - float(4000 + i * interval)) / 65.0f;
      z += amplitude * std::exp(-distance * distance);
    }
    d.update(t, 0, 0, z);
  }
}

int main() {
  StepDetector still;
  feed(still, 0, 18000, 0);
  assert(still.ready && !still.calibrationFailed && still.count == 0);
  assert(std::abs(still.baseline - 9.81f) < .001f);

  StepDetector regular;
  feed(regular, 0, 17000, 3.5f);
  assert(regular.count == 20);

  StepDetector weak;
  feed(weak, 0, 17000, .4f);
  assert(weak.count == 0);

  StepDetector tooFast;
  feed(tooFast, 0, 10000, 3.5f, 200);
  assert(tooFast.count > 0 && tooFast.count < 20);

  // Gap menghapus lingkungan puncak; jumlah dan baseline tidak hilang.
  const auto before = regular.count;
  regular.discardHistory();
  regular.update(17500, 0, 0, 9.81f);
  assert(regular.count == before && regular.ready && regular.gaps == 1);
  regular.update(17520, std::numeric_limits<float>::quiet_NaN(), 0, 9.81f);
  regular.update(17540, 0, 0, 9.81f);
  assert(regular.count == before && std::isfinite(regular.filtered));

  StepDetector insufficient;
  for (unsigned t = 0; t < 100; t += 20) insufficient.update(t, 0, 0, 9.81f);
  insufficient.update(3000, 0, 0, 9.81f);
  assert(insufficient.calibrationFailed && !insufficient.ready && insufficient.count == 0);

  regular.reset();
  assert(!regular.ready && regular.count == 0 && regular.gaps == 0);
  puts("PASS: stationary, 20 peaks, weak peaks, interval, gap/NaN, calibration failure, reset");
}
