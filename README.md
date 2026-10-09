# Chapter 9 — Integrasi Feedback & Tampilan

Target: **DFRobot FireBeetle 2 ESP32-C5**, Arduino ESP32 core **3.x**. Contoh mengikuti pin dan model Chapter 8, serta detector langkah Chapter 4. Gunakan **Serial Monitor 115200 baud**.

## Urutan praktik

| Folder sketch | Praktik | Library tambahan |
| --- | --- | --- |
| `Feedback_LED_Buzzer` | Uji warna dan beep dengan perintah Serial, tanpa sensor/model | Adafruit NeoPixel |
| `Step_Counter_Integrasi` | MPU6050 + counting + TinyML + feedback + Serial; OLED dan OTA opsional | MPU6050, NeoPixel, model Chapter 7; SSD1306/GFX jika OLED aktif |
| `OTA_Basic` | Upload pertama lewat USB, kemudian update lewat Wi-Fi | WiFi dan ArduinoOTA dari ESP32 core |

Buka `.ino` yang namanya sama dengan folder sketch. Pada contoh integrasi, **ketiga header harus tetap dalam folder yang sama**:

- `Config.h`: pin, pilihan OLED/OTA, confidence, versi dan kredensial.
- `StepDetector.h`: parameter dan algoritma counting.
- `ModelWorker.h`: helper inference dan queue agar sampling tidak menunggu model.

## Wiring

| Komponen | Koneksi |
| --- | --- |
| MPU6050 VCC / GND | 3V3 / GND |
| MPU6050 SDA / SCL | GPIO2 / GPIO3 |
| MPU6050 AD0 | GND, alamat `0x68` |
| WS2812, 1 pixel | Data GPIO27; catu daya sesuai modul kit; GND bersama |
| Buzzer pasif HW-508 | Sinyal GPIO6; catu daya/GND sesuai modul kit |
| OLED opsional SSD1306 **128×32** | SDA GPIO2, SCL GPIO3, alamat `0x3C`; daya sesuai modul |

RGB adalah **WS2812 pada kit**, berbeda dari LED_BUILTIN GPIO15. Pin buzzer mengikuti Chapter 8 terbaru (GPIO6), bukan GPIO23 pada contoh awal Chapter 2. Untuk KY-012 aktif, ubah `BUZZER_ACTIVE = true` di sketch feedback atau `Config.h`. Periksa polaritas dan gunakan driver sesuai kebutuhan arus modul.

Pin I2C diatur eksplisit karena default board berbeda dari GPIO2/GPIO3. Pastikan sensor yang dipasang cocok dengan library **Adafruit MPU6050**.

## Persiapan library dan model

Melalui Library Manager, install **Adafruit MPU6050**, **Adafruit Unified Sensor**, **Adafruit BusIO**, dan **Adafruit NeoPixel**. Untuk OLED, tambahkan **Adafruit SSD1306** dan **Adafruit GFX Library**.

Tambahkan ZIP model dari Chapter 7 melalui **Sketch → Include Library → Add .ZIP Library**:

`Chapter_7/Code/ei-xflash-project-1-arduino-1.0.1-impulse-#2.zip`

Header: `xflash-project-1_inferencing.h`. Model yang dipakai memiliki label **diam, jalan, lari**, sampling **50 Hz**, **100 sampel XYZ** per window (~2 detik), dan **300 float** buffer input. Input berupa **m/s² termasuk gravitasi**, sama dengan acquisition Chapter 6. MPU6050 memakai accel ±8 g dan filter 10 Hz. Jangan memasukkan magnitude, gyro, timestamp, unit g, atau mengurangi gravitasi pada jalur model.

## Praktik 1 — feedback

Upload `Feedback_LED_Buzzer`. Kirim `d`, `j`, `l`, `u` lewat Serial:

| Perintah / aktivitas | RGB | Buzzer |
| --- | --- | --- |
| d / diam | Biru | Mati |
| j / jalan | Hijau | Mati |
| l / lari | Merah | Beep 150 ms |
| u / unknown | Kuning redup | Mati |

Kirim `l` berulang cepat untuk melihat pembatasan beep: 150 ms bunyi + minimal 1000 ms cooldown. Contoh ini tidak membutuhkan IMU, model atau Wi-Fi. Perintah yang tidak dikenal, termasuk newline, diabaikan.

## Praktik 2 — sistem terintegrasi

1. Biarkan `USE_OLED = 0`. Kredensial Wi-Fi/OTA kosong membuat contoh berjalan **offline**. Pilih board **DFRobot FireBeetle 2 ESP32-C5**, port USB, dan opsi USB CDC yang sesuai koneksi board, lalu upload.
2. Kirim `r` setelah Serial Monitor terbuka. Diam **3 detik**; tunggu pesan `MULAI bergerak`.
3. Lakukan **20 langkah** dan minta pasangan menghitung manual. Untuk sambungan USB, gunakan stepping di tempat dengan kabel aman. Bandingkan `steps` dengan hitungan manual pada interval yang sama.
4. Ulangi diam, pelan, biasa, cepat, dan goyangan tanpa langkah. Reset dan kalibrasi sebelum setiap percobaan.
5. Amati `aktivitas`, RGB, dan beep. Label stabil memerlukan tiga window valid: sekitar **6 detik setelah kalibrasi**, atau sekitar **9 detik sejak reset**, ditambah pemrosesan. Counting mulai setelah kalibrasi, tanpa menunggu label.

Perintah Serial:

| Perintah | Fungsi |
| --- | --- |
| `r` | Reset hitungan, baseline, history detector, window model dan voting |
| `p` | Toggle mode Serial Plotter; status periodik dimatikan ketika plotting |
| `s` | Cetak status sekarang, di luar mode plotting |

Mode Plotter mengeluarkan `filtered`, `high`, `low`, `steps` setiap pembacaan. Lepas centang `steps` pada legenda ketika ingin membandingkan sinyal dan threshold pada skala yang jelas. Kirim `p` lagi untuk kembali ke log biasa.

Log normal setiap 1 detik berisi `t` (ms), `steps`, `aktivitas` (voting), `raw`, `conf` (confidence raw), `cal`, `gap`, `sensor_err`, `ml_drop`, waktu `DSP`/`NN` (ms), dan `versi`. `gap` menghitung slot penuh 20 ms yang terlewat; `ml_drop` menghitung window model yang gagal masuk queue, bukan jumlah langkah hilang. Nilai gap 0 belum membuktikan jitter 0.

### Dua jalur pemrosesan

Counting: magnitude `sqrt(ax² + ay² + az²)` → moving average 5 sampel → kurangi baseline median saat diam → puncak lokal >**1,2 m/s²** → terima jika sinyal telah turun <**0,3 m/s²** dan jeda antarpuncak ≥**300 ms**. Puncak sebelumnya dikonfirmasi oleh sampel berikutnya. Parameter ada di `StepDetector.h`; ini adaptasi detector Chapter 4 dengan penambahan pembuangan history ketika sensor/timing gagal.

Aktivitas: XYZ mentah → full window baru 100 sampel → `run_classifier()` pada task prioritas 0 → confidence **>0,80** → mayoritas minimal **2 dari 3** window valid. Unknown membersihkan voting dan mematikan buzzer. Hasil yang tidak diperbarui lebih dari 7 detik menjadi unknown. Lari stabil menghasilkan beep setelah cooldown; tidak ada beep per langkah.

**Satu label jalan bukan satu langkah.** Counter tidak digate oleh model: counting tetap berjalan saat aktivitas unknown. Goyangan non-langkah masih dapat menghasilkan false step; uji akurasi counting dan klasifikasi secara terpisah.

Loop Arduino prioritas 1 dan task model prioritas 0 **berbagi CPU ESP32-C5**. Queue menyalin window agar worker tidak mengubah buffer yang sedang diisi. Jika terlambat satu slot penuh, kode membuang history detector/window/voting dan membaca satu sampel aktual, tanpa catch-up palsu. Jumlah dan baseline yang sudah tersedia dipertahankan. Hasil model dari sebelum gap/reset ditolak melalui nomor sesi (`epoch`). Ini masih perlu pengukuran timing pada hardware.

### OLED opsional

Ubah `USE_OLED` menjadi `1` di `Config.h`. Contoh memakai SSD1306 **128×32**, bukan 128×64. Sensor dan OLED berbagi I2C 400 kHz. Refresh setiap 1 detik hanya dicoba jika tersisa minimal 16 ms sebelum sampel berikutnya; transfer 512 byte membutuhkan sekitar 12 ms. Modul lambat, timeout, Wi-Fi dan log tetap bisa menimbulkan gap yang perlu diperiksa.

Jika memiliki OLED 128×64, sesuaikan konstruktor, tata letak dan anggaran transfer; transfer frame penuh lebih lama. Kegagalan alokasi OLED membuat kode melanjutkan Serial. `begin()` sukses belum menjamin modul I2C terpasang: periksa alamat, tegangan dan wiring jika layar gelap.

### Kalibrasi dan evaluasi

Ubah satu parameter per percobaan. Bandingkan high **0,8 / 1,2 / 1,8** pada low 0,3 dan jeda 300 ms. Kemudian kembalikan high 1,2 dan coba jeda 500 ms. Selalu reset, diam 3 detik, lalu uji gerakan yang sama. Jeda 500 ms dapat menolak langkah cepat. Bila sampel baseline <30, status `cal=failed`; perbaiki timing/wiring dan reset.

Untuk manual >0: `error = |device − manual| / manual × 100%`. Contoh 20 manual dan 22 device → 10%. Saat manual =0, laporkan false steps per menit. Catat posisi sensor, parameter, gerakan, timing, dan hasil ulangan. Uji kembali dengan sesi/orang baru setelah tuning.

## Praktik 3 — OTA sederhana

1. Buka `OTA_Basic`; isi `WIFI_SSID`, `WIFI_PASSWORD`, dan **`OTA_PASSWORD`**. Password OTA berbeda dari password Wi-Fi. Placeholder kosong tidak mengaktifkan layanan OTA.
2. Pilih partisi yang mendukung OTA. **Default 4 MB** board yang divalidasi memiliki dua slot aplikasi, masing-masing **1.310.720 byte**, dan `otadata`. Jangan pilih partisi No OTA. Image firmware harus muat pada satu slot.
3. Upload pertama melalui USB. Hubungkan laptop dan ESP32 ke LAN/hotspot yang sama. Tunggu pesan `OTA siap` dan IP pada Serial.
4. Pilih network port **exilir-ch9-ota** di Arduino IDE. Ubah `FIRMWARE_VERSION` dari `9.0.0` ke `9.0.1`, lalu Upload dan masukkan password OTA.
5. Setelah reboot, periksa perubahan versi pada Serial. USB boleh tetap dipasang untuk daya/log; upload menggunakan network port. OTA tidak membuat Serial Monitor menjadi nirkabel.

Pada `Step_Counter_Integrasi`, isi kredensial di `Config.h`, biarkan `USE_OTA = 1`, dan upload awal melalui USB. Network port bernama **exilir-ch9-counter**. Saat transfer, buzzer dimatikan, RGB ungu, task model dihentikan sementara dan counter dijeda. Setelah reboot **count kembali 0** dan perlu diam 3 detik lagi. Error OTA juga memulai ulang sesi. Data counter hanya di RAM.

Firmware baru harus tetap memuat ArduinoOTA agar update selanjutnya tersedia. Model ikut diperbarui sebagai bagian firmware setelah mengganti library model dan compile ulang; OTA tidak melatih model. Contoh ini ditujukan untuk praktik pada LAN yang dipercaya, memakai password OTA, tanpa tambahan TLS atau penandatanganan firmware.

Jika port tidak muncul, cek IP, jaringan, mDNS, firewall dan client isolation. Error auth → cek password OTA; begin/size → cek partisi dan ukuran; connect/receive → cek jaringan/daya. Pemulihan lewat USB tetap tersedia.

## Compile dari root workspace

```powershell
arduino-cli compile --fqbn esp32:esp32:dfrobot_firebeetle2_esp32c5 Chapter_9/Code/Feedback_LED_Buzzer

arduino-cli compile --fqbn esp32:esp32:dfrobot_firebeetle2_esp32c5 --library 'Chapter_7/Code/ei-xflash-project-1-arduino-1.0.1-impulse-#2/xflash-project-1_inferencing' Chapter_9/Code/Step_Counter_Integrasi

arduino-cli compile --fqbn esp32:esp32:dfrobot_firebeetle2_esp32c5 Chapter_9/Code/OTA_Basic
```

Untuk memvalidasi OLED melalui CLI tanpa mengubah `Config.h`, tambahkan `--build-property 'compiler.cpp.extra_flags=-DUSE_OLED=1'`. Untuk tanpa OTA, gunakan `-DUSE_OTA=0`. Hasil aktual dan keterbatasan dicatat di `VALIDASI.md`.

## Referensi

- Lokal: detector Chapter 4; acquisition Chapter 6; metadata/model Chapter 7; inference, pin dan mapping Chapter 8.
- [Contoh BasicOTA Espressif](https://github.com/espressif/arduino-esp32/blob/master/libraries/ArduinoOTA/examples/BasicOTA/BasicOTA.ino); kode disesuaikan dengan core lokal 3.3.11.
- [Partisi Arduino ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html).

## Tambahan — dashboard MQTT

Contoh **[Step_Counter_MQTT](Step_Counter_MQTT/README.md)** mengirim jumlah langkah, label aktivitas, confidence dan skor tiap kelas ke **[dashboard HTML/CSS/JS](../Dashboard/README.md)** melalui broker publik EMQX. Varian ini memakai PubSubClient dan ArduinoJson 7. Buka `../Dashboard/index.html` untuk mencoba demo lokal, lalu ikuti README untuk menghubungkan ESP32. [Hasil validasi MQTT/dashboard](../Dashboard/VALIDASI.md) terpisah dari contoh OLED/OTA di atas.
