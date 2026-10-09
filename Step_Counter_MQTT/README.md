# Step Counter + TinyML + MQTT

Varian Chapter 9 untuk mengirim hasil model **diam/jalan/lari** dan jumlah langkah ke **[dashboard HTML/CSS/JS](../../Dashboard/README.md)**. Menggunakan model Chapter 7, input Chapter 6, pin/feedback Chapter 8, dan detector langkah Chapter 4/9.

Target: **DFRobot FireBeetle 2 ESP32-C5**, ESP32 Arduino core **3.x**. Contoh ini mencakup MPU6050, counter, TinyML, RGB dan buzzer, dengan keluaran Serial serta MQTT. Praktik OLED/OTA tersedia terpisah di `Step_Counter_Integrasi` dan `OTA_Basic`.

## Library

Install melalui Library Manager:

- **Adafruit MPU6050**, **Adafruit Unified Sensor**, **Adafruit BusIO**.
- **Adafruit NeoPixel**.
- **PubSubClient** (divalidasi 2.8).
- **ArduinoJson** versi **7** (divalidasi 7.4.3).

Tambahkan ZIP model Chapter 7 lewat **Sketch → Include Library → Add .ZIP Library**:

`Chapter_7/Code/ei-xflash-project-1-arduino-1.0.1-impulse-#2.zip`

Header model: `xflash-project-1_inferencing.h`. Model yang disertakan memiliki 3 sumbu XYZ mentah **m/s² termasuk gravitasi**, 50 Hz, 100 sampel/window dan label diam/jalan/lari. Dashboard mengikuti label tersebut; perubahan nama kelas memerlukan pembaruan schema dan tampilan.

## Wiring dan konfigurasi

| Komponen | Pin |
| --- | --- |
| MPU6050 SDA / SCL | GPIO2 / GPIO3 |
| MPU6050 VCC / GND / AD0 | 3V3 / GND / GND (alamat 0x68) |
| WS2812 1 pixel | GPIO27, daya sesuai modul, GND bersama |
| Buzzer pasif HW-508 | GPIO6, daya/GND sesuai modul kit |

Untuk buzzer aktif KY-012, ubah `BUZZER_ACTIVE = true`. WS2812 kit berbeda dari LED_BUILTIN GPIO15.

Isi kredensial Wi-Fi pada **[Config.h](Config.h)**:

```cpp
const char *WIFI_SSID = "NAMA_WIFI";
const char *WIFI_PASSWORD = "PASSWORD_WIFI";
const char *MQTT_HOST = "broker.emqx.io";
constexpr uint16_t MQTT_PORT = 1883;
```

SSID kosong membuat sketch tetap menghitung dan menjalankan model secara offline. Jangan memasukkan kredensial ke payload MQTT atau menyertakannya saat membagikan kode.

Upload file **[Step_Counter_MQTT.ino](Step_Counter_MQTT.ino)** lewat USB, buka Serial Monitor 115200, lalu salin `# TOPIC=` ke dashboard. Sketch membuat topic otomatis:

```text
exilir/aiot/ch9/exilir-c5-<12 karakter ID board>/telemetry
```

Kirim `r`, diam 3 detik, tunggu pesan `MULAI bergerak`, lalu uji langkah/aktivitas. `p` mengaktifkan/menonaktifkan Serial Plotter; `s` mencetak status. WebSocket default dashboard adalah `wss://broker.emqx.io:8084/mqtt`, bukan port TCP 1883.

RGB: diam biru, jalan hijau, lari merah, unknown kuning redup. Buzzer berbunyi 150 ms pada hasil lari stabil dengan cooldown. Counter tidak bergantung pada label model.

## File dan alur

| File | Tanggung jawab |
| --- | --- |
| `Step_Counter_MQTT.ino` | Sampling 20 ms, kalibrasi/counting, voting, feedback dan snapshot telemetry |
| `Config.h` | Wi-Fi, broker, interval dan pin |
| `StepDetector.h` | Baseline, moving average, threshold dan jeda antarlangkah |
| `ModelWorker.h` | Queue window dan inference pada task prioritas 0 |
| `MqttPublisher.h` | Queue snapshot, Wi-Fi/MQTT reconnect dan publish JSON pada task prioritas 0 |

Loop Arduino prioritas 1 dan kedua worker berbagi CPU ESP32-C5. Connect/reconnect MQTT tidak ditempatkan pada loop sampling; queue menyalin snapshot supaya worker tidak membaca state yang sedang berubah. Queue hanya menyimpan snapshot terakhir. Sampling tetap perlu diukur pada board, terutama saat logging, reconnect dan inference berjalan bersamaan.

JSON dikirim sekitar setiap 1 detik menggunakan MQTT 3.1.1, QoS 0, tanpa retain. Topic per board tidak menjamin privasi atau autentikasi pada broker publik. Jalur Arduino menggunakan TCP tanpa TLS dan ditujukan untuk data latihan. Lihat [panduan dashboard](../../Dashboard/README.md) untuk format payload, status dan troubleshooting.

## Compile CLI

Dari root workspace, setelah library terpasang:

```powershell
arduino-cli compile --fqbn esp32:esp32:dfrobot_firebeetle2_esp32c5 --library 'Chapter_7/Code/ei-xflash-project-1-arduino-1.0.1-impulse-#2/xflash-project-1_inferencing' Chapter_9/Code/Step_Counter_MQTT
```

Compile pada ESP32 core 3.3.11 berhasil: flash **1.205.949 byte (92%)** pada slot default 1.310.720 byte; global **55.376 byte (16%)**. Angka global tidak mencakup peak heap/stack runtime. Menambah model/library perlu pemeriksaan ukuran ulang. Hasil dan batas pengujian: **[VALIDASI dashboard/MQTT](../../Dashboard/VALIDASI.md)**.
