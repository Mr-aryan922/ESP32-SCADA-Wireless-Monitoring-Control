// ============================================================
//  XBOTS PROJECT — ESP32 NODE 2 (SERVER + WEB DASHBOARD)
//  Receives data from Node 1 via ESP-NOW
//  Hosts Web Dashboard on local IP (Wi-Fi AP or STA mode)
//  Display: 1.3" OLED SH1106 (128x64)
// ============================================================

#include <Wire.h>
#include <U8g2lib.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>    // v6.x — install via Library Manager

// ──────────────────────────────────────────────
//  WiFi CREDENTIALS (for Station mode)
//  OR comment out & use AP mode below
// ──────────────────────────────────────────────
#define WIFI_SSID   "YOUR_WIFI_SSID"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"

// If you want Access Point mode instead, set AP_MODE = true
#define AP_MODE     false
#define AP_SSID     "XBOTS_MONITOR"
#define AP_PASS     "xbots1234"

// ──────────────────────────────────────────────
//  OLED
// ──────────────────────────────────────────────
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// ──────────────────────────────────────────────
//  WEB SERVER
// ──────────────────────────────────────────────
WebServer server(80);

// ──────────────────────────────────────────────
//  SHARED DATA STRUCTURE (must match Node 1)
// ──────────────────────────────────────────────
typedef struct SensorData {
  float temperature;
  float humidity;
  int   gasLevel;
  bool  ir1Detected;
  bool  ir2Detected;
  bool  relay1State;
  bool  relay2State;
  char  status[32];
} SensorData;

SensorData receivedData;
volatile bool newDataFlag = false;
unsigned long lastReceived = 0;

// ──────────────────────────────────────────────
//  ESP-NOW RECEIVE CALLBACK
// ──────────────────────────────────────────────
void onDataReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len == sizeof(SensorData)) {
    memcpy(&receivedData, incomingData, sizeof(SensorData));
    newDataFlag = true;
    lastReceived = millis();
  }
}

// ──────────────────────────────────────────────
//  OLED DISPLAY
// ──────────────────────────────────────────────
String serverIP = "";

void updateOLED() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tr);

  // Header
  display.drawBox(0, 0, 128, 10);
  display.setDrawColor(0);
  display.drawStr(2, 8, "XBOTS SERVER");
  display.setDrawColor(1);

  // IP Address
  display.setFont(u8g2_font_5x7_tr);
  String ipLine = "IP:" + serverIP;
  display.drawStr(0, 20, ipLine.c_str());

  // Data
  display.setFont(u8g2_font_6x10_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%",
           receivedData.temperature, receivedData.humidity);
  display.drawStr(0, 33, buf);

  snprintf(buf, sizeof(buf), "Gas:%d %s",
           receivedData.gasLevel,
           receivedData.gasLevel > 300 ? "ALERT" : "OK   ");
  display.drawStr(0, 44, buf);

  snprintf(buf, sizeof(buf), "IR1:%s IR2:%s R1:%s R2:%s",
           receivedData.ir1Detected ? "Y" : "N",
           receivedData.ir2Detected ? "Y" : "N",
           receivedData.relay1State ? "ON" : "OF",
           receivedData.relay2State ? "ON" : "OF");
  display.drawStr(0, 55, buf);

  display.sendBuffer();
}

void showServerBoot() {
  display.clearBuffer();
  display.setFont(u8g2_font_logisoso28_tr);
  int w = display.getStrWidth("XBOTS");
  display.drawStr((128-w)/2, 44, "XBOTS");
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(14, 58, "SERVER NODE 2");
  display.sendBuffer();
  delay(2000);

  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(10, 22, "Starting WiFi...");
  display.drawStr(10, 38, "Web Server...");
  display.drawStr(10, 54, "ESP-NOW...");
  display.sendBuffer();
  delay(1000);
}

// ──────────────────────────────────────────────
//  DASHBOARD HTML
// ──────────────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XBOTS Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@400;600;700&display=swap');
  :root {
    --bg: #0a0d12;
    --surface: #111620;
    --card: #161d2a;
    --border: #1e2d45;
    --accent: #00d4ff;
    --accent2: #00ff9d;
    --warn: #ff6b35;
    --danger: #ff2d55;
    --text: #c8d8e8;
    --muted: #4a6080;
    --green: #00ff9d;
    --red: #ff2d55;
  }
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Rajdhani', sans-serif;
    min-height: 100vh;
    font-size: 15px;
  }
  /* Scanline overlay */
  body::before {
    content:'';
    position:fixed;
    inset:0;
    background: repeating-linear-gradient(0deg, transparent, transparent 2px, rgba(0,212,255,0.015) 2px, rgba(0,212,255,0.015) 4px);
    pointer-events:none;
    z-index:1000;
  }
  header {
    display:flex; align-items:center; justify-content:space-between;
    padding: 16px 28px;
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    position: sticky; top:0; z-index:100;
  }
  .logo {
    font-family: 'Share Tech Mono', monospace;
    font-size: 22px;
    color: var(--accent);
    letter-spacing: 6px;
    text-shadow: 0 0 20px rgba(0,212,255,0.5);
  }
  .logo span { color: var(--accent2); }
  .status-bar {
    display:flex; align-items:center; gap:16px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 12px;
  }
  .dot {
    width:8px; height:8px; border-radius:50%;
    background: var(--accent2);
    box-shadow: 0 0 8px var(--accent2);
    animation: pulse 2s infinite;
  }
  .dot.offline { background: var(--muted); box-shadow:none; animation:none; }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.4} }
  #last-update { color: var(--muted); font-size:11px; }

  main { padding: 24px 28px; max-width: 1200px; }

  /* Section title */
  .section-title {
    font-family: 'Share Tech Mono', monospace;
    font-size: 11px;
    letter-spacing: 3px;
    color: var(--muted);
    text-transform: uppercase;
    margin-bottom: 12px;
    margin-top: 28px;
    padding-bottom: 6px;
    border-bottom: 1px solid var(--border);
  }

  /* Metric Grid */
  .metrics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 16px;
  }
  .metric-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 20px;
    position: relative;
    overflow: hidden;
    transition: border-color .3s;
  }
  .metric-card::before {
    content: '';
    position:absolute; top:0; left:0; right:0; height:2px;
    background: linear-gradient(90deg, var(--accent), transparent);
  }
  .metric-card:hover { border-color: rgba(0,212,255,0.3); }
  .metric-label {
    font-family: 'Share Tech Mono', monospace;
    font-size: 10px;
    letter-spacing: 2px;
    color: var(--muted);
    text-transform: uppercase;
    margin-bottom: 8px;
  }
  .metric-value {
    font-size: 36px;
    font-weight: 700;
    color: var(--accent);
    line-height: 1;
    font-family: 'Share Tech Mono', monospace;
  }
  .metric-unit {
    font-size: 14px;
    color: var(--muted);
    margin-left: 4px;
  }
  .metric-sub {
    margin-top: 8px;
    font-size: 12px;
    color: var(--muted);
  }
  .metric-card.warning .metric-value { color: var(--warn); }
  .metric-card.warning::before { background: linear-gradient(90deg, var(--warn), transparent); }
  .metric-card.danger .metric-value { color: var(--danger); }
  .metric-card.danger::before { background: linear-gradient(90deg, var(--danger), transparent); }
  .metric-card.success .metric-value { color: var(--green); }
  .metric-card.success::before { background: linear-gradient(90deg, var(--green), transparent); }

  /* Status rows */
  .status-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 16px;
  }
  .status-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px 20px;
    display: flex;
    align-items: center;
    gap: 16px;
  }
  .status-icon {
    width: 48px; height: 48px;
    border-radius: 8px;
    display:flex; align-items:center; justify-content:center;
    font-size: 22px;
    background: var(--surface);
    border: 1px solid var(--border);
    flex-shrink: 0;
  }
  .status-info { flex:1; }
  .status-name {
    font-weight: 600;
    font-size: 15px;
    margin-bottom: 4px;
  }
  .status-badge {
    display: inline-block;
    padding: 2px 10px;
    border-radius: 4px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1px;
  }
  .badge-on {
    background: rgba(0,255,157,0.15);
    color: var(--green);
    border: 1px solid rgba(0,255,157,0.3);
  }
  .badge-off {
    background: rgba(74,96,128,0.2);
    color: var(--muted);
    border: 1px solid var(--border);
  }
  .badge-detected {
    background: rgba(0,212,255,0.15);
    color: var(--accent);
    border: 1px solid rgba(0,212,255,0.3);
  }
  .badge-clear {
    background: rgba(74,96,128,0.2);
    color: var(--muted);
    border: 1px solid var(--border);
  }
  .badge-alert {
    background: rgba(255,45,85,0.15);
    color: var(--danger);
    border: 1px solid rgba(255,45,85,0.3);
    animation: flash 1s infinite;
  }
  @keyframes flash { 0%,100%{opacity:1} 50%{opacity:.5} }

  /* Gas bar */
  .gas-bar-wrap {
    margin-top: 10px;
    height: 6px;
    background: var(--surface);
    border-radius: 3px;
    overflow:hidden;
  }
  .gas-bar-fill {
    height:100%;
    border-radius: 3px;
    transition: width .5s, background .5s;
    background: var(--green);
  }

  /* System Status Panel */
  .system-row {
    display:flex; align-items:center; justify-content:space-between;
    padding: 10px 0;
    border-bottom: 1px solid var(--border);
  }
  .system-row:last-child { border-bottom:none; }
  .system-label { font-size:13px; color: var(--muted); font-family:'Share Tech Mono',monospace; }
  .system-val { font-size:13px; font-family:'Share Tech Mono',monospace; color: var(--accent2); }

  footer {
    text-align:center;
    padding: 20px;
    font-family:'Share Tech Mono',monospace;
    font-size:11px;
    color: var(--muted);
    border-top: 1px solid var(--border);
    margin-top: 40px;
  }
  @media(max-width:600px) {
    .status-grid { grid-template-columns:1fr; }
    .metrics-grid { grid-template-columns:1fr 1fr; }
    main { padding:16px; }
  }
</style>
</head>
<body>
<header>
  <div class="logo">X<span>BOTS</span></div>
  <div class="status-bar">
    <div id="conn-dot" class="dot offline"></div>
    <span id="conn-text" style="color:var(--muted)">Waiting...</span>
    <span id="last-update">--</span>
  </div>
</header>

<main>
  <div class="section-title">// ENVIRONMENTAL SENSORS</div>
  <div class="metrics-grid">

    <div class="metric-card" id="card-temp">
      <div class="metric-label">Temperature</div>
      <div>
        <span class="metric-value" id="temp-val">--</span>
        <span class="metric-unit">°C</span>
      </div>
      <div class="metric-sub" id="temp-sub">Awaiting data...</div>
    </div>

    <div class="metric-card" id="card-hum">
      <div class="metric-label">Humidity</div>
      <div>
        <span class="metric-value" id="hum-val">--</span>
        <span class="metric-unit">%</span>
      </div>
      <div class="metric-sub" id="hum-sub">Awaiting data...</div>
    </div>

    <div class="metric-card" id="card-gas">
      <div class="metric-label">Gas Level (MQ-2)</div>
      <div>
        <span class="metric-value" id="gas-val">--</span>
        <span class="metric-unit">/ 4095</span>
      </div>
      <div class="gas-bar-wrap">
        <div class="gas-bar-fill" id="gas-bar" style="width:0%"></div>
      </div>
      <div class="metric-sub" id="gas-sub">Awaiting data...</div>
    </div>

    <div class="metric-card" id="card-status">
      <div class="metric-label">System Status</div>
      <div class="metric-value" id="status-val" style="font-size:18px;margin-top:6px">--</div>
      <div class="metric-sub" id="status-sub">Awaiting data...</div>
    </div>

  </div>

  <div class="section-title">// IR SENSORS & RELAY STATUS</div>
  <div class="status-grid">

    <div class="status-card">
      <div class="status-icon">IR</div>
      <div class="status-info">
        <div class="status-name">IR Sensor 1</div>
        <span class="status-badge badge-clear" id="ir1-badge">CLEAR</span>
        <div class="metric-sub" style="margin-top:4px">Relay 1 trigger</div>
      </div>
    </div>

    <div class="status-card">
      <div class="status-icon">IR</div>
      <div class="status-info">
        <div class="status-name">IR Sensor 2</div>
        <span class="status-badge badge-clear" id="ir2-badge">CLEAR</span>
        <div class="metric-sub" style="margin-top:4px">Relay 2 trigger</div>
      </div>
    </div>

    <div class="status-card">
      <div class="status-icon">R1</div>
      <div class="status-info">
        <div class="status-name">Relay 1 — Motor A</div>
        <span class="status-badge badge-off" id="r1-badge">OFF</span>
        <div class="metric-sub" id="r1-gpio" style="margin-top:4px">GPIO 32 → Node 1</div>
      </div>
    </div>

    <div class="status-card">
      <div class="status-icon">R2</div>
      <div class="status-info">
        <div class="status-name">Relay 2 — Motor B</div>
        <span class="status-badge badge-off" id="r2-badge">OFF</span>
        <div class="metric-sub" id="r2-gpio" style="margin-top:4px">GPIO 33 → Node 1</div>
      </div>
    </div>

  </div>

  <div class="section-title">// SYSTEM INFO</div>
  <div class="metric-card" style="padding:8px 20px;">
    <div class="system-row">
      <span class="system-label">NODE 1 (Sensor)</span>
      <span class="system-val" id="sys-node1">--</span>
    </div>
    <div class="system-row">
      <span class="system-label">NODE 2 (Server IP)</span>
      <span class="system-val" id="sys-node2">--</span>
    </div>
    <div class="system-row">
      <span class="system-label">Protocol</span>
      <span class="system-val">ESP-NOW (2.4 GHz)</span>
    </div>
    <div class="system-row">
      <span class="system-label">Refresh Rate</span>
      <span class="system-val">1 Hz (1 sec)</span>
    </div>
    <div class="system-row">
      <span class="system-label">Uptime</span>
      <span class="system-val" id="sys-uptime">--</span>
    </div>
  </div>
</main>

<footer>
  XBOTS &nbsp;|&nbsp; SMART ENVIRONMENT MONITOR &nbsp;|&nbsp; ESP-NOW v2
</footer>

<script>
let startTime = Date.now();

function fetchData() {
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      // Connection status
      let dot = document.getElementById('conn-dot');
      let connText = document.getElementById('conn-text');
      if (d.connected) {
        dot.className = 'dot';
        connText.style.color = 'var(--accent2)';
        connText.textContent = 'LIVE';
      } else {
        dot.className = 'dot offline';
        connText.style.color = 'var(--muted)';
        connText.textContent = 'No Data';
      }
      document.getElementById('last-update').textContent =
        'Last: ' + new Date().toLocaleTimeString();

      // Temperature
      let t = d.temperature.toFixed(1);
      document.getElementById('temp-val').textContent = t;
      let cardT = document.getElementById('card-temp');
      cardT.className = 'metric-card';
      if (d.temperature > 35) { cardT.classList.add('danger'); document.getElementById('temp-sub').textContent = 'HIGH — Check ventilation'; }
      else if (d.temperature > 28) { cardT.classList.add('warning'); document.getElementById('temp-sub').textContent = 'Elevated'; }
      else { cardT.classList.add('success'); document.getElementById('temp-sub').textContent = 'Normal range'; }

      // Humidity
      document.getElementById('hum-val').textContent = d.humidity.toFixed(0);
      let cardH = document.getElementById('card-hum');
      cardH.className = 'metric-card';
      if (d.humidity > 80) { cardH.classList.add('warning'); document.getElementById('hum-sub').textContent = 'High humidity'; }
      else if (d.humidity < 30) { cardH.classList.add('warning'); document.getElementById('hum-sub').textContent = 'Low humidity'; }
      else { cardH.classList.add('success'); document.getElementById('hum-sub').textContent = 'Comfortable'; }

      // Gas
      document.getElementById('gas-val').textContent = d.gasLevel;
      let pct = Math.min(100, (d.gasLevel / 4095) * 100);
      let bar = document.getElementById('gas-bar');
      bar.style.width = pct.toFixed(1) + '%';
      let cardG = document.getElementById('card-gas');
      cardG.className = 'metric-card';
      if (d.gasLevel > 500) {
        cardG.classList.add('danger');
        bar.style.background = 'var(--danger)';
        document.getElementById('gas-sub').textContent = 'DANGER — Evacuate!';
      } else if (d.gasLevel > 300) {
        cardG.classList.add('warning');
        bar.style.background = 'var(--warn)';
        document.getElementById('gas-sub').textContent = 'WARNING — Gas detected';
      } else {
        cardG.classList.add('success');
        bar.style.background = 'var(--green)';
        document.getElementById('gas-sub').textContent = 'Air quality normal';
      }

      // Status
      document.getElementById('status-val').textContent = d.status;
      document.getElementById('status-sub').textContent =
        d.gasLevel > 300 ? 'Immediate action required!' : 'System operating normally';

      // IR Sensors
      let i1 = document.getElementById('ir1-badge');
      i1.className = 'status-badge ' + (d.ir1 ? 'badge-detected' : 'badge-clear');
      i1.textContent = d.ir1 ? 'DETECTED' : 'CLEAR';

      let i2 = document.getElementById('ir2-badge');
      i2.className = 'status-badge ' + (d.ir2 ? 'badge-detected' : 'badge-clear');
      i2.textContent = d.ir2 ? 'DETECTED' : 'CLEAR';

      // Relays
      let r1 = document.getElementById('r1-badge');
      r1.className = 'status-badge ' + (d.relay1 ? 'badge-on' : 'badge-off');
      r1.textContent = d.relay1 ? 'ON' : 'OFF';

      let r2 = document.getElementById('r2-badge');
      r2.className = 'status-badge ' + (d.relay2 ? 'badge-on' : 'badge-off');
      r2.textContent = d.relay2 ? 'ON' : 'OFF';

      // System info
      document.getElementById('sys-node1').textContent =
        d.connected ? 'Connected via ESP-NOW' : 'No signal';
      document.getElementById('sys-node2').textContent = d.serverIP;

      // Uptime
      let sec = Math.floor((Date.now() - startTime) / 1000);
      let h = Math.floor(sec/3600), m = Math.floor((sec%3600)/60), s = sec%60;
      document.getElementById('sys-uptime').textContent =
        String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
    })
    .catch(e => {
      document.getElementById('conn-dot').className = 'dot offline';
      document.getElementById('conn-text').textContent = 'Error';
    });
}

fetchData();
setInterval(fetchData, 1000);
</script>
</body>
</html>
)rawliteral";

// ──────────────────────────────────────────────
//  WEB ROUTES
// ──────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", DASHBOARD_HTML);
}

void handleData() {
  bool connected = (millis() - lastReceived < 5000);  // Timeout 5s

  DynamicJsonDocument doc(512);
  doc["temperature"] = receivedData.temperature;
  doc["humidity"]    = receivedData.humidity;
  doc["gasLevel"]    = receivedData.gasLevel;
  doc["ir1"]         = receivedData.ir1Detected;
  doc["ir2"]         = receivedData.ir2Detected;
  doc["relay1"]      = receivedData.relay1State;
  doc["relay2"]      = receivedData.relay2State;
  doc["status"]      = receivedData.status;
  doc["connected"]   = connected;
  doc["serverIP"]    = serverIP;

  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ──────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  // Init OLED
  display.begin();
  showServerBoot();

  // ── WiFi Setup ──
  if (AP_MODE) {
    WiFi.softAP(AP_SSID, AP_PASS);
    serverIP = WiFi.softAPIP().toString();
    Serial.print("AP Mode IP: "); Serial.println(serverIP);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
      delay(500); Serial.print(".");
      tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      serverIP = WiFi.localIP().toString();
      Serial.print("\nConnected! IP: "); Serial.println(serverIP);
    } else {
      Serial.println("\nWiFi failed — switching to AP mode");
      WiFi.softAP(AP_SSID, AP_PASS);
      serverIP = WiFi.softAPIP().toString();
    }
  }

  // Print MAC for Node 1 configuration
  Serial.print("Node2 MAC Address: ");
  Serial.println(WiFi.macAddress());

  // ── ESP-NOW Init ──
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_recv_cb(onDataReceive);

  // ── Web Server Routes ──
  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.begin();

  // Init received data with safe defaults
  receivedData.temperature = 0;
  receivedData.humidity    = 0;
  receivedData.gasLevel    = 0;
  receivedData.ir1Detected = false;
  receivedData.ir2Detected = false;
  receivedData.relay1State = false;
  receivedData.relay2State = false;
  snprintf(receivedData.status, sizeof(receivedData.status), "Waiting for Node1");

  Serial.println("Server ready!");
  Serial.print("Dashboard: http://"); Serial.println(serverIP);

  // Show IP on OLED
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 12, "SERVER READY!");
  display.drawStr(0, 26, "Dashboard at:");
  String ipStr = "http://" + serverIP;
  display.drawStr(0, 40, ipStr.c_str());
  display.drawStr(0, 54, "Waiting Node1...");
  display.sendBuffer();
  delay(3000);
}

// ──────────────────────────────────────────────
//  MAIN LOOP
// ──────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Update OLED every second
  static unsigned long lastOLED = 0;
  if (millis() - lastOLED >= 1000) {
    updateOLED();
    lastOLED = millis();
  }

  // Serial debug output
  if (newDataFlag) {
    newDataFlag = false;
    Serial.printf("Received → T:%.1f H:%.1f Gas:%d IR1:%d IR2:%d R1:%d R2:%d\n",
                  receivedData.temperature, receivedData.humidity,
                  receivedData.gasLevel,
                  receivedData.ir1Detected, receivedData.ir2Detected,
                  receivedData.relay1State, receivedData.relay2State);
  }
}
