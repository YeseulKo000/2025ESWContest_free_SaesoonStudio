// ===== TEST SKETCH + WiFi/Server attach (Fan via L298N IN1/IN2) =====

#include <WiFi.h>
#include <HTTPClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

// ---------- Pins ----------
#define DHTPIN 27
#define SOIL_MOISTURE_PIN 34
#define LIGHT_SENSOR_PIN 35
#define WATER_TRIG_PIN 5
#define WATER_ECHO_PIN 4

// [UPDATE] Fan via L298N (ENA jumpered)
#define FAN_IN1_PIN 32
#define FAN_IN2_PIN 33

#define WATER_PUMP_PIN 26
#define NUTRIENT_PUMP_PIN 14
#define PIXEL_1_PIN 2
#define PIXEL_2_PIN 21
#define NUM_PIXELS_PER_STRIP 8
#define BUTTON1_PIN 16
#define BUTTON2_PIN 17
#define BUTTON3_PIN 18
#define BUTTON4_PIN 19

// ---------- Thresholds / timings ----------
#define DHTTYPE DHT11
const float TEMP_THRESHOLD_HIGH = 28.0;
const float HUMID_THRESHOLD_HIGH = 70.0;
const int   LIGHT_THRESHOLD_DARK = 2000;

const unsigned long WATERING_DURATION_MS = 5000;               // 수동/자동 급수 유지시간
const unsigned long NUTRIENT_SUPPLY_INTERVAL_MS = 10UL * 24 * 60 * 60 * 1000;
const unsigned long NUTRIENT_SUPPLY_DURATION_MS = 3000;

// ---------- Globals ----------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_NeoPixel pixel1(NUM_PIXELS_PER_STRIP, PIXEL_1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixel2(NUM_PIXELS_PER_STRIP, PIXEL_2_PIN, NEO_GRB + NEO_KHZ800);

float currentHumidity = 0;
float currentTemperature = 0;
int   currentSoilMoistureRaw = 0;
int   currentSoilMoisturePercent = 0;
int   currentLightValue = 0;
float waterTankDistance = 0;

enum ControlMode { AUTO_MODE, MANUAL_ON, MANUAL_OFF };
ControlMode fanControlMode = AUTO_MODE;
ControlMode waterControlMode = AUTO_MODE;
ControlMode lightControlMode = AUTO_MODE;
ControlMode nutrientControlMode = AUTO_MODE;

bool isFanOn = false;
bool isWaterPumpOn = false;
bool isLedLightOn = false;
bool isNutrientPumpOn = false;

unsigned long lastDHTReadTime = 0;
const unsigned long DHT_READ_INTERVAL_MS = 2000;
unsigned long lastSoilMoistureReadTime = 0;
const unsigned long SOIL_MOISTURE_READ_INTERVAL_MS = 5000;
unsigned long lastLightReadTime = 0;
const unsigned long LIGHT_READ_INTERVAL_MS = 1000;
unsigned long lastUltrasonicReadTime = 0;
const unsigned long ULTRASONIC_READ_INTERVAL_MS = 3000;

unsigned long lastWateringStartTime = 0;
unsigned long lastNutrientSupplyTime = 0;

// ---------- Buttons (debounced, release-triggered) ----------
struct Btn {
  uint8_t pin; int raw; int stable;
  unsigned long lastChangeMs; unsigned long pressStartMs;
  unsigned long presses;
};
const unsigned long DEBOUNCE_MS   = 50;
const unsigned long LONG_PRESS_MS = 2000;
Btn buttons[4] = {
  {BUTTON1_PIN, HIGH, HIGH, 0, 0, 0},
  {BUTTON2_PIN, HIGH, HIGH, 0, 0, 0},
  {BUTTON3_PIN, HIGH, HIGH, 0, 0, 0},
  {BUTTON4_PIN, HIGH, HIGH, 0, 0, 0}
};

// ===== Wi-Fi / Server Config & sending =====
const char* WIFI_SSID = "PlantoryAP";         
const char* WIFI_PASS = "saesoonstudio";     
const char* SERVER_HOST = "192.168.4.1";   // ← 나중에 아이피 변경
const int   SERVER_PORT = 8080;              
const char* SERVER_PATH = "/sensor";         

unsigned long lastDataSendTime = 0;
const unsigned long DATA_SEND_INTERVAL_MS = 10000; // 10초마다 전송(원하면 변경)
// ================================================

// ---------- Prototypes ----------
void readButtons();
void readDHTSensor();
void readSoilMoistureSensor();
void readLightSensor();
void readTankLevels();
void controlFanAutomatic();
void controlWaterPumpAutomatic();
void controlLedLightAutomatic();
void controlNutrientPumpAutomatic();
void setFanManual(bool);
void setWaterPumpManual(bool);
void setLedLightManual(bool);
void setNutrientPumpManual(bool);
void setAllPixels(bool);

// [ADD]
void connectWiFi();
void sendDataToServer();

// [UPDATE] Fan helpers
inline void fanOn()  { digitalWrite(FAN_IN1_PIN, HIGH); digitalWrite(FAN_IN2_PIN, LOW);  }
inline void fanOff() { digitalWrite(FAN_IN1_PIN, LOW);  digitalWrite(FAN_IN2_PIN, LOW);  } // coast stop
// ======================================================================
// setup
// ======================================================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  pixel1.begin(); pixel2.begin();
  pixel1.setBrightness(25); pixel2.setBrightness(25);

  // [UPDATE] Fan pins
  pinMode(FAN_IN1_PIN, OUTPUT);
  pinMode(FAN_IN2_PIN, OUTPUT);

  pinMode(WATER_PUMP_PIN, OUTPUT);
  pinMode(NUTRIENT_PUMP_PIN, OUTPUT);
  pinMode(WATER_TRIG_PIN, OUTPUT);
  pinMode(WATER_ECHO_PIN, INPUT);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);

  // 초기 버튼 상태 동기화
  for (int i=0;i<4;i++){
    int now = digitalRead(buttons[i].pin);
    buttons[i].raw = now;
    buttons[i].stable = now;
    buttons[i].lastChangeMs = millis();
    buttons[i].pressStartMs = 0;
  }

  // [UPDATE] Fan OFF(initial)
  fanOff();
  digitalWrite(WATER_PUMP_PIN, LOW);
  digitalWrite(NUTRIENT_PUMP_PIN, LOW);
  setAllPixels(false);

  Serial.println("System Initialized (TEST MODE + WiFi/Server attach; Fan via L298N IN1/IN2)");

  // Wi-Fi 연결 시도
  connectWiFi();
}

// ======================================================================
// loop
// ======================================================================
void loop() {
  unsigned long now = millis();

  // 센서 폴링
  if (now - lastDHTReadTime >= DHT_READ_INTERVAL_MS) { readDHTSensor(); lastDHTReadTime = now; }
  if (now - lastSoilMoistureReadTime >= SOIL_MOISTURE_READ_INTERVAL_MS) { readSoilMoistureSensor(); lastSoilMoistureReadTime = now; }
  if (now - lastLightReadTime >= LIGHT_READ_INTERVAL_MS) { readLightSensor(); lastLightReadTime = now; }
  if (now - lastUltrasonicReadTime >= ULTRASONIC_READ_INTERVAL_MS) { readTankLevels(); lastUltrasonicReadTime = now; }

  // ========== 모드 스위치 ==========
  // 팬
  switch (fanControlMode) {
    case AUTO_MODE:
      controlFanAutomatic(); break;
    case MANUAL_ON:
      if (!isFanOn) { fanOn(); isFanOn = true; Serial.println("Fan MANUAL ON"); }
      break;
    case MANUAL_OFF:
      if (isFanOn) { fanOff(); isFanOn = false; Serial.println("Fan MANUAL OFF"); }
      break;
  }

  // 급수 펌프
  switch (waterControlMode) {
    case AUTO_MODE:
      controlWaterPumpAutomatic(); break;
    case MANUAL_ON:
      if (!isWaterPumpOn) {
        digitalWrite(WATER_PUMP_PIN, HIGH);
        isWaterPumpOn = true;
        lastWateringStartTime = now;
        Serial.println("WaterPump MANUAL ON");
      }
      if (isWaterPumpOn && (now - lastWateringStartTime >= WATERING_DURATION_MS)) {
        digitalWrite(WATER_PUMP_PIN, LOW);
        isWaterPumpOn = false;
        waterControlMode = AUTO_MODE;
        Serial.println("WaterPump MANUAL OFF (timeout) -> AUTO");
      }
      break;
    case MANUAL_OFF:
      if (isWaterPumpOn) { digitalWrite(WATER_PUMP_PIN, LOW); isWaterPumpOn = false; Serial.println("WaterPump MANUAL OFF"); }
      break;
  }

  // LED
  switch (lightControlMode) {
    case AUTO_MODE:
      controlLedLightAutomatic(); break;
    case MANUAL_ON:
      if (!isLedLightOn) { setAllPixels(true); isLedLightOn = true; Serial.println("LED MANUAL ON"); }
      break;
    case MANUAL_OFF:
      if (isLedLightOn) { setAllPixels(false); isLedLightOn = false; Serial.println("LED MANUAL OFF"); }
      break;
  }

  // 영양제 펌프
  switch (nutrientControlMode) {
    case AUTO_MODE:
      controlNutrientPumpAutomatic(); break;
    case MANUAL_ON:
      if (!isNutrientPumpOn) {
        digitalWrite(NUTRIENT_PUMP_PIN, HIGH);
        isNutrientPumpOn = true;
        lastNutrientSupplyTime = now;
        Serial.println("Nutrient MANUAL ON");
      }
      if (isNutrientPumpOn && (now - lastNutrientSupplyTime >= NUTRIENT_SUPPLY_DURATION_MS)) {
        digitalWrite(NUTRIENT_PUMP_PIN, LOW);
        isNutrientPumpOn = false;
        nutrientControlMode = AUTO_MODE;
        Serial.println("Nutrient MANUAL OFF (timeout) -> AUTO");
      }
      break;
    case MANUAL_OFF:
      if (isNutrientPumpOn) { digitalWrite(NUTRIENT_PUMP_PIN, LOW); isNutrientPumpOn = false; Serial.println("Nutrient MANUAL OFF"); }
      break;
  }

  // 버튼 폴링
  readButtons();

  // 주기적 서버 전송
  if (millis() - lastDataSendTime >= DATA_SEND_INTERVAL_MS) {
    sendDataToServer();
    lastDataSendTime = millis();
  }
}

// ======================================================================
// Functions
// ======================================================================

void readButtons() {
  unsigned long now = millis();
  for (int i=0;i<4;i++){
    int r = digitalRead(buttons[i].pin);
    if (r != buttons[i].raw) {
      buttons[i].raw = r;
      buttons[i].lastChangeMs = now;
    }
    if ((now - buttons[i].lastChangeMs) >= DEBOUNCE_MS && buttons[i].stable != buttons[i].raw) {
      int prev = buttons[i].stable, cur = buttons[i].raw;
      buttons[i].stable = cur;

      // 눌림 시작
      if (prev==HIGH && cur==LOW) {
        buttons[i].pressStartMs = now;
      }
      // 릴리즈
      if (prev==LOW && cur==HIGH) {
        unsigned long held = buttons[i].pressStartMs ? now - buttons[i].pressStartMs : 0;
        buttons[i].pressStartMs = 0;

        Serial.print("BTN"); Serial.print(i+1); Serial.print(" released, held(ms)="); Serial.println(held);

        if (i==0) { // Fan
          setFanManual(!isFanOn);
        } else if (i==1) { // Water pump
          setWaterPumpManual(!isWaterPumpOn);
        } else if (i==2) { // Nutrient pump
          setNutrientPumpManual(!isNutrientPumpOn);
        } else if (i==3) { // LED / ALL AUTO
          if (held >= LONG_PRESS_MS) {
            fanControlMode = AUTO_MODE;
            waterControlMode = AUTO_MODE;
            lightControlMode = AUTO_MODE;
            nutrientControlMode = AUTO_MODE;
            Serial.println("ALL AUTO (BTN4 long press)");
          } else {
            setLedLightManual(!isLedLightOn);
          }
        }
      }
    }
  }
}

void readDHTSensor() {
  currentHumidity = dht.readHumidity();
  currentTemperature = dht.readTemperature();
  Serial.print("DHT: "); Serial.print(currentTemperature); Serial.print("C, ");
  Serial.print(currentHumidity); Serial.println("%");
}

void readSoilMoistureSensor() {
  currentSoilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
  currentSoilMoisturePercent = map(currentSoilMoistureRaw, 2500, 1000, 0, 100);
  if (currentSoilMoisturePercent < 0) currentSoilMoisturePercent = 0;
  if (currentSoilMoisturePercent > 100) currentSoilMoisturePercent = 100;
  Serial.print("Soil: raw="); Serial.print(currentSoilMoistureRaw);
  Serial.print(" pct="); Serial.println(currentSoilMoisturePercent);
}

void readLightSensor() {
  currentLightValue = analogRead(LIGHT_SENSOR_PIN);
  Serial.print("Light: "); Serial.println(currentLightValue);
}

void readTankLevels() {
  digitalWrite(WATER_TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(WATER_TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(WATER_TRIG_PIN, LOW);
  unsigned long dur = pulseIn(WATER_ECHO_PIN, HIGH, 23200);
  if (dur == 0) waterTankDistance = 9999;
  else waterTankDistance = dur * 0.034 / 2;
  Serial.print("Tank(cm): "); Serial.println(waterTankDistance);
}

// ----- AUTO controllers -----
void controlFanAutomatic() {
  if (fanControlMode != AUTO_MODE) return;
  if (currentTemperature > TEMP_THRESHOLD_HIGH || currentHumidity > HUMID_THRESHOLD_HIGH) {
    if (!isFanOn) { fanOn(); isFanOn = true; Serial.println("Fan ON (auto)"); }
  } else {
    if (isFanOn) { fanOff(); isFanOn = false; Serial.println("Fan OFF (auto)"); }
  }
}

void controlWaterPumpAutomatic() {
  if (waterControlMode != AUTO_MODE) return;
  if (!isWaterPumpOn && currentSoilMoisturePercent < 25) {
    digitalWrite(WATER_PUMP_PIN, HIGH); isWaterPumpOn = true;
    lastWateringStartTime = millis();
    Serial.println("Pump ON (auto)");
  }
  if (isWaterPumpOn && millis() - lastWateringStartTime >= WATERING_DURATION_MS) {
    digitalWrite(WATER_PUMP_PIN, LOW); isWaterPumpOn = false;
    Serial.println("Pump OFF (auto)");
  }
}

void controlLedLightAutomatic() {
  if (lightControlMode != AUTO_MODE) return;
  if (currentLightValue < LIGHT_THRESHOLD_DARK) {
    if (!isLedLightOn) { setAllPixels(true); isLedLightOn = true; Serial.println("LED ON (auto)"); }
  } else {
    if (isLedLightOn) { setAllPixels(false); isLedLightOn = false; Serial.println("LED OFF (auto)"); }
  }
}

void controlNutrientPumpAutomatic() {
  if (nutrientControlMode != AUTO_MODE) return;
  if (!isNutrientPumpOn && millis() - lastNutrientSupplyTime >= NUTRIENT_SUPPLY_INTERVAL_MS) {
    digitalWrite(NUTRIENT_PUMP_PIN, HIGH); isNutrientPumpOn = true;
    lastNutrientSupplyTime = millis();
    Serial.println("Nutrient ON (auto)");
  }
  if (isNutrientPumpOn && millis() - lastNutrientSupplyTime >= NUTRIENT_SUPPLY_DURATION_MS) {
    digitalWrite(NUTRIENT_PUMP_PIN, LOW); isNutrientPumpOn = false;
    Serial.println("Nutrient OFF (auto)");
  }
}

// ----- Manual setters -----
void setFanManual(bool turnOn)            { fanControlMode = turnOn ? MANUAL_ON : MANUAL_OFF; }
void setWaterPumpManual(bool turnOn)      { waterControlMode = turnOn ? MANUAL_ON : MANUAL_OFF; }
void setLedLightManual(bool turnOn)       { lightControlMode = turnOn ? MANUAL_ON : MANUAL_OFF; }
void setNutrientPumpManual(bool turnOn)   { nutrientControlMode = turnOn ? MANUAL_ON : MANUAL_OFF; }

// ----- NeoPixel -----
void setAllPixels(bool turnOn) {
  uint32_t color = turnOn ? pixel1.Color(255, 0, 0) : pixel1.Color(0, 0, 0);
  for (int i=0;i<NUM_PIXELS_PER_STRIP;i++) {
    pixel1.setPixelColor(i, color);
    pixel2.setPixelColor(i, color);
  }
  pixel1.show(); pixel2.show();
}

// ===== Wi-Fi / HTTP helpers =====
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
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

void sendDataToServer() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skip send: WiFi not connected.");
    return;
  }
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SERVER_PATH;

  String body = "{";
  body += "\"air_temperature\":" + String(currentTemperature) + ",";
  body += "\"air_humidity\":" + String(currentHumidity) + ",";
  body += "\"soil_moisture\":" + String(currentSoilMoistureRaw) + ",";
  body += "\"light_intensity\":" + String(currentLightValue) + ",";
  body += "\"water_level\":" + String(waterTankDistance);
  body += "}";

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  Serial.print("POST "); Serial.print(url); Serial.print(" -> ");
  int code = http.POST(body);
  Serial.print("HTTP "); Serial.println(code);
  if (code > 0) {
    String res = http.getString();
    Serial.println(res);
  }
  http.end();
}
