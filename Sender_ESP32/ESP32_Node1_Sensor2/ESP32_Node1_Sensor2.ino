// ============================================================
//  XBOTS PROJECT — ESP32 NODE 1 (SENSOR + OLED)
//  Sensors: DHT11, MQ2, IR x2 | Actuators: Relay x2
//  Display: 1.3" OLED SH1106 (128x64)
//  Communication: ESP-NOW → Node 2 (Server)
// ============================================================

#include <Wire.h>
#include <U8g2lib.h>        // For 1.3" SH1106 OLED
#include <DHT.h>
#include <esp_now.h>
#include <WiFi.h>

// ──────────────────────────────────────────────
//  PIN DEFINITIONS
// ──────────────────────────────────────────────
#define DHT_PIN       4     // DHT11 data pin
#define DHT_TYPE      DHT11
#define MQ2_PIN       34    // MQ2 analog output (ADC1)
#define IR1_PIN       26    // IR Sensor 1 digital output
#define IR2_PIN       27    // IR Sensor 2 digital output
#define RELAY1_PIN    32    // Relay 1 (IN1)
#define RELAY2_PIN    33    // Relay 2 (IN2)

// ──────────────────────────────────────────────
//  REPLACE WITH YOUR NODE 2 (SERVER) MAC ADDRESS
//  Run Node2 sketch first, note Serial output MAC
// ──────────────────────────────────────────────
uint8_t serverMACAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// Example: {0x3C, 0x61, 0x05, 0x0A, 0xB2, 0x44}

// ──────────────────────────────────────────────
//  OBJECTS
// ──────────────────────────────────────────────
// SH1106 1.3" OLED – I2C (SDA=21, SCL=22 on ESP32)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHT_PIN, DHT_TYPE);

// ──────────────────────────────────────────────
//  ESP-NOW DATA STRUCTURE (must match Node 2)
// ──────────────────────────────────────────────
typedef struct SensorData {
  float temperature;
  float humidity;
  int   gasLevel;
  bool  ir1Detected;
  bool  ir2Detected;
  bool  relay1State;
  bool  relay2State;
  char  status[32];      // short status string
} SensorData;

SensorData sensorData;
bool espNowConnected = false;

esp_now_peer_info_t peerInfo;

// ──────────────────────────────────────────────
//  ESP-NOW SEND CALLBACK
// ──────────────────────────────────────────────
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    espNowConnected = true;
  } else {
    espNowConnected = false;
  }
}

// ──────────────────────────────────────────────
//  OLED HELPER: SPLASH SCREENS
// ──────────────────────────────────────────────
void showTeamSplash() {
  // ── Frame 1: XBOTS big logo ──
  display.clearBuffer();
  display.setFont(u8g2_font_logisoso28_tr);
  int w = display.getStrWidth("XBOTS");
  display.drawStr((128 - w) / 2, 44, "XBOTS");
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(30, 58, "TEAM PRESENTS");
  display.sendBuffer();
  delay(1000);

  // ── Frame 2: animated dots ──
  for (int i = 0; i < 3; i++) {
    display.clearBuffer();
    display.setFont(u8g2_font_logisoso28_tr);
    int ww = display.getStrWidth("XBOTS");
    display.drawStr((128 - ww) / 2, 44, "XBOTS");
    display.setFont(u8g2_font_6x10_tr);
    String dots = "";
    for (int d = 0; d <= i; d++) dots += ".";
    display.drawStr(52, 58, dots.c_str());
    display.sendBuffer();
    delay(400);
  }
  delay(600);
}

void showProjectSplash() {
  // ── Frame 1: Project Name sliding in ──
  for (int x = 128; x >= 0; x -= 8) {
    display.clearBuffer();
    display.setFont(u8g2_font_7x14B_tr);
    display.drawStr(x, 22, "SMART ENV");
    display.drawStr(x, 40, "MONITOR");
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(0, 58, "by XBOTS  v1.0");
    display.sendBuffer();
    delay(20);
  }
  delay(1200);
}

void showConnectingScreen() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(10, 16, "Connecting to");
  display.setFont(u8g2_font_7x14B_tr);
  display.drawStr(12, 36, "SERVER ESP32");
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(18, 52, "via ESP-NOW...");
  // Draw signal icon (simple lines)
  display.drawLine(70, 60, 70, 60);
  display.sendBuffer();

  // Animate dots
  for (int i = 0; i < 4; i++) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(10, 16, "Connecting to");
    display.setFont(u8g2_font_7x14B_tr);
    display.drawStr(12, 36, "SERVER ESP32");
    display.setFont(u8g2_font_6x10_tr);
    String progress = "via ESP-NOW";
    for (int d = 0; d < i; d++) progress += ".";
    display.drawStr(18, 52, progress.c_str());
    display.sendBuffer();
    delay(500);
  }
}

// ──────────────────────────────────────────────
//  OLED: LIVE DATA DISPLAY
// ──────────────────────────────────────────────
void showSensorData(float temp, float hum, int gas,
                    bool ir1, bool ir2,
                    bool r1, bool r2) {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);

  // ── Header bar ──
  display.drawBox(0, 0, 128, 10);
  display.setDrawColor(0);
  display.drawStr(2, 8, "XBOTS MONITOR");
  display.setDrawColor(1);

  // ── Temperature & Humidity ──
  display.setFont(u8g2_font_6x10_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "T:%.1fC  H:%.0f%%", temp, hum);
  display.drawStr(0, 22, buf);

  // ── Gas Level ──
  snprintf(buf, sizeof(buf), "Gas: %d  %s", gas,
           gas > 300 ? "!ALERT!" : "Normal");
  display.drawStr(0, 33, buf);

  // ── IR Sensors ──
  snprintf(buf, sizeof(buf), "IR1:%s  IR2:%s",
           ir1 ? "DET" : "---",
           ir2 ? "DET" : "---");
  display.drawStr(0, 44, buf);

  // ── Relay States ──
  snprintf(buf, sizeof(buf), "R1:%s  R2:%s",
           r1 ? "[ON] " : "[OFF]",
           r2 ? "[ON] " : "[OFF]");
  display.drawStr(0, 55, buf);

  // ── ESP-NOW indicator (top right) ──
  display.setFont(u8g2_font_5x7_tr);
  if (espNowConnected) {
    display.drawStr(100, 8, "");   // shown as filled box
    display.drawBox(105, 2, 20, 6);
    display.setDrawColor(0);
    display.drawStr(106, 8, "OK");
    display.setDrawColor(1);
  }

  display.sendBuffer();
}

// ──────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  // Pin modes
  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);  // Relay OFF (active LOW module)
  digitalWrite(RELAY2_PIN, HIGH);

  // Init OLED
  display.begin();
  display.setContrast(200);

  // ── Splash Sequence ──
  showTeamSplash();
  showProjectSplash();

  // Init DHT
  dht.begin();

  // ── Init WiFi for ESP-NOW ──
  WiFi.mode(WIFI_STA);
  Serial.print("Node1 MAC: ");
  Serial.println(WiFi.macAddress());

  showConnectingScreen();

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(0, 32, "ESP-NOW FAILED!");
    display.sendBuffer();
    while (1) delay(1000);
  }

  esp_now_register_send_cb(onDataSent);

  // Register peer (Node 2 / Server)
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, serverMACAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  } else {
    Serial.println("Peer added successfully");
  }

  delay(1000);
}

// ──────────────────────────────────────────────
//  MAIN LOOP
// ──────────────────────────────────────────────
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 1000;  // Send every 1 second

void loop() {
  // ── Read Sensors ──
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  int   gasLevel    = analogRead(MQ2_PIN);  // 0-4095

  // Handle DHT read failure
  if (isnan(temperature)) temperature = -1.0;
  if (isnan(humidity))    humidity    = -1.0;

  // IR: LOW = detected on most modules (check your module)
  bool ir1 = (digitalRead(IR1_PIN) == LOW);
  bool ir2 = (digitalRead(IR2_PIN) == LOW);

  // ── Relay Logic: Actuate based on IR detection ──
  bool relay1State = ir1;   // Relay 1 ON when IR1 detects object
  bool relay2State = ir2;   // Relay 2 ON when IR2 detects object

  // Active LOW relay modules: LOW = ON
  digitalWrite(RELAY1_PIN, relay1State ? LOW : HIGH);
  digitalWrite(RELAY2_PIN, relay2State ? LOW : HIGH);

  // ── Update OLED ──
  showSensorData(temperature, humidity, gasLevel,
                 ir1, ir2, relay1State, relay2State);

  // ── Send via ESP-NOW every SEND_INTERVAL ms ──
  if (millis() - lastSend >= SEND_INTERVAL) {
    sensorData.temperature  = temperature;
    sensorData.humidity     = humidity;
    sensorData.gasLevel     = gasLevel;
    sensorData.ir1Detected  = ir1;
    sensorData.ir2Detected  = ir2;
    sensorData.relay1State  = relay1State;
    sensorData.relay2State  = relay2State;

    if (gasLevel > 300) {
      snprintf(sensorData.status, sizeof(sensorData.status), "GAS ALERT!");
    } else if (ir1 || ir2) {
      snprintf(sensorData.status, sizeof(sensorData.status), "Motion Detected");
    } else {
      snprintf(sensorData.status, sizeof(sensorData.status), "All Normal");
    }

    esp_err_t result = esp_now_send(serverMACAddress,
                                    (uint8_t *)&sensorData,
                                    sizeof(sensorData));

    Serial.printf("T:%.1f H:%.1f Gas:%d IR1:%d IR2:%d R1:%d R2:%d | Send:%s\n",
                  temperature, humidity, gasLevel,
                  ir1, ir2, relay1State, relay2State,
                  result == ESP_OK ? "OK" : "FAIL");

    lastSend = millis();
  }
}
