# Validasi Chapter 9 — 9 Oktober 2026

Lingkungan: Windows, Arduino CLI **1.5.1**, ESP32 core **3.3.11**, board **`esp32:esp32:dfrobot_firebeetle2_esp32c5`**, partisi default 4 MB dengan dua slot OTA. Model dipilih langsung dari folder ekspor Chapter 7 melalui `--library`, sesuai perintah README.

## Compile

| Sketch / konfigurasi | Status | Flash / slot 1.310.720 byte | Global / RAM 327.680 byte |
| --- | --- | --- | --- |
| Feedback_LED_Buzzer | PASS, exit 0 | 327.906 byte (25%) | 17.856 byte (5%) |
| Step_Counter_Integrasi, OLED=0 / OTA=1 | PASS, exit 0 | 1.259.189 byte (96%) | 59.880 byte (18%) |
| Step_Counter_Integrasi, OLED=1 / OTA=1 | PASS, exit 0 | 1.270.949 byte (96%) | 59.952 byte (18%) |
| OTA_Basic | PASS, exit 0 | 1.084.431 byte (82%) | 53.496 byte (16%) |

Default tidak memuat kredensial: layanan OTA hanya dimulai setelah SSID dan password OTA diisi. Compile tidak menguji jaringan atau proses upload OTA. Flash integrasi tersisa **51.531 byte** tanpa OLED atau **39.771 byte** dengan OLED pada slot default; penambahan library/model perlu pemeriksaan ukuran ulang. Angka RAM adalah penggunaan global statis, bukan peak heap/stack runtime. Worker memakai stack 16.384 byte yang dialokasikan saat runtime.

Konfigurasi OLED diuji dengan salinan sketch di folder sementara dan hanya mengubah `USE_OLED` menjadi 1; source utama tetap `USE_OLED=0`, `USE_OTA=1`. Seluruh contoh menggunakan default buzzer pasif HW-508.

Library tersedia: Adafruit MPU6050 **2.2.9**, NeoPixel **1.15.5**, Unified Sensor **1.1.15**, BusIO **1.17.4**, SSD1306 **2.5.17**, GFX **1.12.6**.

## Uji detector dengan sinyal sintetis

`tests/test_step_detector.cpp` dijalankan pada host dan **PASS**:

- Diam menghasilkan 0 dan baseline mendekati 9,81 m/s².
- 20 pulsa yang memenuhi aturan menghasilkan 20 hitungan.
- Pulsa lemah tidak diterima.
- Pulsa dengan interval 200 ms dibatasi oleh jeda minimum 300 ms.
- Gap dan NaN tidak menghasilkan langkah tambahan; baseline/count dipertahankan.
- Kalibrasi dengan sampel kurang dari 30 gagal dengan benar.
- Reset menghapus count, baseline dan status sesi.

Compiler host: Clang, C++17. Pada lingkungan ini MSVC STL memerlukan flag kompatibilitas versi `_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH`. Perintah yang dijalankan:

```powershell
clang++ -std=c++17 -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH Chapter_9/Code/tests/test_step_detector.cpp -o Chapter_9/Code/tests/test_step_detector.exe
./Chapter_9/Code/tests/test_step_detector.exe
```

Sinyal sintetis tidak membuktikan akurasi hitungan langkah manusia.

## Slide dan konsistensi materi

- **62 slide**, PPTX dapat diedit; PDF diekspor melalui Microsoft PowerPoint. Bagian dashboard/MQTT baru berada pada slide 43–57.
- Semua slide memiliki catatan pengajar; isi disalin ke Markdown dan JSON.
- Tidak ada placeholder XXX/YYY, part ZIP ganda, bentuk di luar slide, atau overflow teks pada pemeriksaan final.
- Cover, branding, Q&A dan penutup mengikuti template yang disediakan; template awal disimpan di `../Slide/template/`.
- Metadata model dicocokkan: XYZ, 50 Hz, 100 sampel/window, buffer 300 float dan label diam/jalan/lari.
- Pin mengikuti Chapter 8: I2C2/3, RGB27, buzzer6; counting diadaptasi dari Chapter 4.
- Hasil log/grafik pada slide merupakan ilustrasi. Tabel eksperimen sengaja menyisakan kolom device untuk hasil peserta.
- Screenshot dashboard memakai demo lokal. Compile/UI/TCP MQTT telah diuji; round-trip WSS publik masih timeout dan belum diuji dari board. Rincian pada [../Dashboard/VALIDASI.md](../Dashboard/VALIDASI.md).

## Pengujian kit yang masih perlu dilakukan

Belum upload atau menguji perangkat fisik. Perlu memeriksa pemasangan/pin aktual, sensor, warna WS2812, tipe buzzer, baseline, false steps, akurasi aktivitas, latensi, jitter, heap/stack worker, refresh OLED, serta upload OTA melalui LAN. Saat OTA berjalan, counting dijeda; reboot atau error OTA memulai ulang sesi dan count kembali 0. Parameter threshold harus dievaluasi pada posisi sensor dan pengguna yang sebenarnya.
