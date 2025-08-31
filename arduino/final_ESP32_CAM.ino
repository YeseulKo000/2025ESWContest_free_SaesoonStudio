#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// ====== Wi-Fi / Server ======
const char* WIFI_SSID = "PlantoryAP";     // ← 바꾸세요
const char* WIFI_PASS = "saesoonstudio";     // ← 바꾸세요
const char* SERVER_HOST = "192.168.4.1";      // ← PC 핫스팟/라즈베리파이 AP의 IP
const int   SERVER_PORT = 8080;                  // ← Flask app.py의 PORT 
const char* AI_ENDPOINT  = "/ai";


const unsigned long CAPTURE_INTERVAL_MS = 30 * 1000UL; // 30초마다 촬영/전송
unsigned long lastShot = 0;

// ====== Camera pin map (AI Thinker) ======
#define PWDN_GPIO_NUM       32
#define RESET_GPIO_NUM      -1
#define XCLK_GPIO_NUM       0
#define SIOD_GPIO_NUM       26
#define SIOC_GPIO_NUM       27

#define Y9_GPIO_NUM         35
#define Y8_GPIO_NUM         34
#define Y7_GPIO_NUM         39
#define Y6_GPIO_NUM         36
#define Y5_GPIO_NUM         21
#define Y4_GPIO_NUM         19
#define Y3_GPIO_NUM         18
#define Y2_GPIO_NUM         5
#define VSYNC_GPIO_NUM      25
#define HREF_GPIO_NUM       23
#define PCLK_GPIO_NUM       22

String buildUrl(const char* host, int port, const char* path) {
  String url = "http://";
  url += host;
  url += ":";
  url += String(port);
  url += path;
  return url;
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed.");
  }
}

bool cameraInit() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // 해상도/품질은 네트워크/안정성에 맞게 조정
  config.frame_size   = FRAMESIZE_VGA;  // QVGA, VGA, SVGA, XGA...
  config.jpeg_quality = 20;            // ↑수치=더압축(용량↓). 10~30 권장
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x\n", err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    // 필요 시 색보정/밝기 조절 등
    // s->set_brightness(s, 0);
    // s->set_contrast(s, 0);
  }
  Serial.println("Camera init OK");
  return true;
}

bool postJpeg(camera_fb_t* fb) {
  ensureWifi();
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = buildUrl(SERVER_HOST, SERVER_PORT, AI_ENDPOINT);
  HTTPClient http;
  http.setTimeout(15000);
  http.begin(url);
  http.addHeader("Content-Type", "application/octet-stream"); // 또는 "image/jpeg"
  int code = http.POST(fb->buf, fb->len);
  Serial.print("POST /ai -> HTTP ");
  Serial.println(code);
  if (code > 0) {
    String res = http.getString();
    Serial.println(res);
  }
  http.end();
  return (code == 200);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  ensureWifi();
  cameraInit();
}

void loop() {
  unsigned long now = millis();
  if (now - lastShot >= CAPTURE_INTERVAL_MS) {
    lastShot = now;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    } else {
      bool ok = postJpeg(fb);
      esp_camera_fb_return(fb);
      if (!ok) {
        Serial.println("POST failed.");
      }
    }
  }
  delay(50);
}