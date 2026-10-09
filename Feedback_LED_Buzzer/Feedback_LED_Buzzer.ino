/* Chapter 9 - uji feedback tanpa sensor/model.
 * ESP32 core 3.x; WS2812 GPIO27, buzzer pasif HW-508 GPIO6.
 * Serial 115200: d=diam, j=jalan, l=lari, u=unknown.
 */
#include <Adafruit_NeoPixel.h>

constexpr uint8_t RGB_PIN = 27;
constexpr uint8_t BUZZER_PIN = 6;
constexpr bool BUZZER_ACTIVE = false; // true untuk KY-012 aktif
constexpr uint32_t BEEP_MS = 150;
constexpr uint32_t COOLDOWN_MS = 1000;
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);
bool beeping = false, hasBeeped = false;
uint32_t beepStarted = 0;

void buzzer(bool on) {
  if (BUZZER_ACTIVE) digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
  else ledcWriteTone(BUZZER_PIN, on ? 2000 : 0);
}

void showActivity(char command) {
  const char *label;
  uint32_t color;
  switch (command) {
    case 'd': label = "diam"; color = rgb.Color(0, 0, 60); break;
    case 'j': label = "jalan"; color = rgb.Color(0, 60, 0); break;
    case 'l': label = "lari"; color = rgb.Color(60, 0, 0); break;
    case 'u': label = "unknown"; color = rgb.Color(12, 12, 0); break;
    default: return;
  }
  rgb.fill(color);
  rgb.show();
  if (command != 'l') {
    buzzer(false);
    beeping = false;
  } else if (!beeping && (!hasBeeped || millis() - beepStarted >= BEEP_MS + COOLDOWN_MS)) {
    buzzer(true);
    beeping = hasBeeped = true;
    beepStarted = millis();
  }
  Serial.printf("aktivitas=%s buzzer=%s\n", label, beeping ? "beep" : "off");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  rgb.begin();
  rgb.setBrightness(40);
  if (!BUZZER_ACTIVE && !ledcAttach(BUZZER_PIN, 2000, 8)) {
    Serial.println("ERROR: PWM buzzer gagal");
    while (true) delay(100);
  }
  showActivity('u');
  Serial.println("Kirim d / j / l / u. Tidak perlu model atau Wi-Fi.");
}

void loop() {
  if (beeping && millis() - beepStarted >= BEEP_MS) {
    buzzer(false);
    beeping = false;
  }
  if (Serial.available()) showActivity(Serial.read());
  delay(1);
}
