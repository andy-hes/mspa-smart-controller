#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef UART_BAUD
#define UART_BAUD 9600
#endif
#ifndef UART_RX_PIN
#define UART_RX_PIN 16
#endif
#ifndef UART_TX_PIN
#define UART_TX_PIN 17
#endif

static const uint8_t START_BYTE = 0xA5;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 120000;
static const uint32_t RESTORE_DELAY_MS = 60000;
static const uint32_t ONLINE_TIMEOUT_MS = 10000;
static const uint32_t REMOTE_STATUS_GUARD_MS = 90000;
static const char* AP_SSID = "MSpa-Setup";
static const char* AP_PASSWORD = "mspasetup";

struct SpaState {
  bool filter_on = true;
  bool heater_on = true;
  bool auto_restore_enabled = true;
  bool desired_run = true;
  uint8_t bubbles_level = 0;
  uint8_t target_temp = 38;
  float current_temp_c = 0.0f;
  uint8_t bath_status = 0;
  bool online = false;
  bool ap_mode = false;
  uint32_t last_rx_ms = 0;
  bool restore_pending = true;
  uint32_t boot_ms = 0;
  uint32_t restore_guard_until_ms = 0;
  uint8_t temp_multiplier = 1;
};

SpaState state;
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

String wifiSsid;
String wifiPassword;

uint8_t checksum(uint8_t cmd, uint8_t value) {
  return static_cast<uint8_t>((START_BYTE + cmd + value) & 0xFF);
}

void saveSpaSettings() {
  prefs.begin("mspa", false);
  prefs.putBool("filter", state.filter_on);
  prefs.putBool("heater", state.heater_on);
  prefs.putBool("auto", state.auto_restore_enabled);
  prefs.putBool("desired", state.desired_run);
  prefs.putUChar("bubbles", state.bubbles_level);
  prefs.putUChar("target", state.target_temp);
  prefs.end();
}

void loadSpaSettings() {
  prefs.begin("mspa", true);
  state.filter_on = prefs.getBool("filter", true);
  state.heater_on = prefs.getBool("heater", true);
  state.auto_restore_enabled = prefs.getBool("auto", true);
  state.desired_run = prefs.getBool("desired", true);
  state.bubbles_level = prefs.getUChar("bubbles", 0);
  state.target_temp = prefs.getUChar("target", 38);
  prefs.end();
}

void saveWifiSettings(const String& ssid, const String& password) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.end();
}

void loadWifiSettings() {
  prefs.begin("wifi", true);
  wifiSsid = prefs.getString("ssid", WIFI_SSID);
  wifiPassword = prefs.getString("pass", WIFI_PASSWORD);
  prefs.end();
}

void ensureSafeHeaterState() {
  if (!state.filter_on) {
    state.heater_on = false;
  }
}

void markDesiredRun(bool enabled) {
  state.desired_run = enabled;
  if (!enabled) {
    state.auto_restore_enabled = false;
    state.heater_on = false;
    state.filter_on = false;
  } else {
    state.auto_restore_enabled = true;
    state.filter_on = true;
    state.heater_on = true;
  }
  saveSpaSettings();
}

void sendFrame(uint8_t cmd, uint8_t value) {
  uint8_t frame[4] = {START_BYTE, cmd, value, checksum(cmd, value)};
  Serial2.write(frame, 4);
  Serial.printf("TX %02X %02X %02X %02X\n", frame[0], frame[1], frame[2], frame[3]);
}

void handleRestore() {
  if (!state.auto_restore_enabled) return;
  if (!state.desired_run) return;
  if (!state.online) return;

  state.filter_on = true;
  state.heater_on = true;
  ensureSafeHeaterState();
  state.restore_guard_until_ms = millis() + 30000;
  saveSpaSettings();
}

void jsonError(AsyncWebServerRequest* req, int code, const char* message) {
  AsyncResponseStream* response = req->beginResponseStream("application/json");
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  serializeJson(doc, *response);
  req->send(response, code);
}

void writeStatus(JsonDocument& doc) {
  doc["ok"] = true;
  doc["online"] = state.online;
  doc["current_temperature_c"] = state.current_temp_c;
  doc["target_temperature_c"] = state.target_temp;
  doc["filter_on"] = state.filter_on;
  doc["heater_on"] = state.heater_on;
  doc["bubbles_level"] = state.bubbles_level;
  doc["auto_restore_enabled"] = state.auto_restore_enabled;
  doc["desired_run"] = state.desired_run;
  doc["bath_status"] = state.bath_status;
  doc["uptime_s"] = millis() / 1000;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["ap_mode"] = state.ap_mode;
  doc["ip"] = state.ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["ssid"] = state.ap_mode ? String(AP_SSID) : WiFi.SSID();
}

void jsonStatus(AsyncWebServerRequest* req) {
  AsyncResponseStream* response = req->beginResponseStream("application/json");
  JsonDocument doc;
  writeStatus(doc);
  serializeJson(doc, *response);
  req->send(response);
}

String htmlPage() {
  JsonDocument doc;
  writeStatus(doc);
  String status;
  serializeJson(doc, status);

  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>MSpa Controller</title><style>";
  html += "body{font-family:Arial,sans-serif;margin:0;background:#10212b;color:#f5fbff}";
  html += "main{max-width:760px;margin:auto;padding:22px}";
  html += ".card{background:#183442;border:1px solid #2b5868;border-radius:8px;padding:16px;margin:14px 0}";
  html += "button,input{font:inherit;padding:10px;border-radius:6px;border:0;margin:4px}";
  html += "input{width:calc(100% - 28px)}button{background:#33a6c9;color:white}pre{white-space:pre-wrap}";
  html += "</style></head><body><main><h1>MSpa Controller</h1>";
  html += "<div class='card'><h2>Status</h2><pre id='status'>";
  html += status;
  html += "</pre></div>";
  html += "<div class='card'><h2>Control</h2>";
  html += "<button onclick=\"post('/api/filter/on')\">Filter on</button><button onclick=\"post('/api/filter/off')\">Filter off</button>";
  html += "<button onclick=\"post('/api/heater/on')\">Heater on</button><button onclick=\"post('/api/heater/off')\">Heater off</button>";
  html += "<button onclick=\"post('/api/bubbles/on')\">Bubbles on</button><button onclick=\"post('/api/bubbles/off')\">Bubbles off</button>";
  html += "<button onclick=\"post('/api/restore')\">Restore</button></div>";
  html += "<div class='card'><h2>Setpoint</h2><form method='post' action='/api/target-temperature'>";
  html += "<input name='value' type='number' min='15' max='40' step='1' value='" + String(state.target_temp) + "'>";
  html += "<button>Save setpoint</button></form></div>";
  html += "<div class='card'><h2>Wi-Fi</h2><form method='post' action='/api/wifi'>";
  html += "<input name='ssid' placeholder='SSID' value='" + wifiSsid + "'>";
  html += "<input name='password' type='password' placeholder='Password'>";
  html += "<button>Save Wi-Fi and restart</button></form></div>";
  html += "<script>async function post(u){await fetch(u,{method:'POST'});location.reload()}</script>";
  html += "</main></body></html>";
  return html;
}

void setFilter(bool enabled) {
  state.filter_on = enabled;
  ensureSafeHeaterState();
  state.desired_run = state.filter_on || state.heater_on;
  saveSpaSettings();
}

void setHeater(bool enabled) {
  if (enabled) {
    state.filter_on = true;
    state.desired_run = true;
  }
  state.heater_on = enabled;
  if (!state.heater_on && !state.filter_on) {
    state.desired_run = false;
  }
  saveSpaSettings();
}

void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", htmlPage());
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    jsonStatus(req);
  });

  server.on("/api/filter/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    setFilter(true);
    jsonStatus(req);
  });

  server.on("/api/filter/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    setFilter(false);
    jsonStatus(req);
  });

  server.on("/api/heater/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    setHeater(true);
    jsonStatus(req);
  });

  server.on("/api/heater/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    setHeater(false);
    jsonStatus(req);
  });

  server.on("/api/bubbles/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.bubbles_level = 1;
    saveSpaSettings();
    jsonStatus(req);
  });

  server.on("/api/bubbles/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.bubbles_level = 0;
    saveSpaSettings();
    jsonStatus(req);
  });

  server.on("/api/target-temperature", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) return jsonError(req, 400, "missing value");
    int v = req->getParam("value", true)->value().toInt();
    if (v < 15 || v > 40) return jsonError(req, 400, "value out of range");
    state.target_temp = static_cast<uint8_t>(v);
    saveSpaSettings();
    jsonStatus(req);
  });

  server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest* req) {
    handleRestore();
    jsonStatus(req);
  });

  server.on("/api/auto-restore/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.auto_restore_enabled = true;
    saveSpaSettings();
    jsonStatus(req);
  });

  server.on("/api/auto-restore/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.auto_restore_enabled = false;
    saveSpaSettings();
    jsonStatus(req);
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("ssid", true)) return jsonError(req, 400, "missing ssid");
    String ssid = req->getParam("ssid", true)->value();
    String password = req->hasParam("password", true) ? req->getParam("password", true)->value() : "";
    saveWifiSettings(ssid, password);
    req->send(200, "text/plain", "Wi-Fi saved. Restarting...");
    delay(500);
    ESP.restart();
  });

  server.onNotFound([](AsyncWebServerRequest* req) {
    if (state.ap_mode) {
      req->redirect("/");
      return;
    }
    jsonError(req, 404, "not found");
  });
}

void handleBathStatus(uint8_t value) {
  uint8_t previous = state.bath_status;
  state.bath_status = value;

  if (millis() < state.restore_guard_until_ms || millis() < REMOTE_STATUS_GUARD_MS) {
    return;
  }

  if (previous == 0x03 && value == 0x00 && state.desired_run) {
    Serial.println("Remote/off status detected. Disabling desired run and auto-restore.");
    markDesiredRun(false);
  } else if (previous == 0x00 && value == 0x03 && !state.desired_run) {
    Serial.println("Remote/on status detected. Re-enabling desired run and auto-restore.");
    markDesiredRun(true);
  }
}

void readFrames() {
  static uint8_t buf[4];
  static uint8_t idx = 0;

  while (Serial2.available()) {
    uint8_t b = static_cast<uint8_t>(Serial2.read());
    if (b == START_BYTE) {
      idx = 0;
      buf[idx++] = b;
      continue;
    }
    if (idx > 0 && idx < 4) {
      buf[idx++] = b;
      if (idx == 4) {
        uint8_t chk = checksum(buf[1], buf[2]);
        if (chk == buf[3]) {
          state.last_rx_ms = millis();
          state.online = true;
          uint8_t cmd = buf[1];
          uint8_t value = buf[2];
          if (cmd == 0x06) {
            state.current_temp_c = value / 2.0f;
          } else if (cmd == 0x08) {
            handleBathStatus(value);
          }
        }
        idx = 0;
      }
    }
  }

  if (state.last_rx_ms > 0 && (millis() - state.last_rx_ms > ONLINE_TIMEOUT_MS)) {
    state.online = false;
  }
}

void writeControlFrames() {
  ensureSafeHeaterState();

  uint8_t filterVal = state.desired_run && state.filter_on ? 0x01 : 0x00;
  uint8_t heaterVal = state.desired_run && state.heater_on && state.filter_on ? 0x01 : 0x00;
  uint8_t bubbleVal = state.bubbles_level;

  sendFrame(0x02, filterVal);
  sendFrame(0x01, heaterVal);
  sendFrame(0x03, bubbleVal);
  sendFrame(0x04, static_cast<uint8_t>(state.target_temp * state.temp_multiplier));
}

void startFallbackAp() {
  state.ap_mode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("Fallback AP started: %s / %s at %s\n", AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

void connectWifiOrFallback() {
  if (wifiSsid.length() == 0) {
    startFallbackAp();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.printf("Connecting Wi-Fi: %s", wifiSsid.c_str());
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    state.ap_mode = false;
    Serial.printf("Wi-Fi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Wi-Fi connection timed out.");
    startFallbackAp();
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  state.boot_ms = millis();
  state.restore_guard_until_ms = RESTORE_DELAY_MS + 30000;
  loadSpaSettings();
  loadWifiSettings();

  connectWifiOrFallback();
  setupRoutes();
  server.begin();
}

void loop() {
  static uint32_t last_tx = 0;

  if (state.ap_mode) {
    dnsServer.processNextRequest();
  }

  readFrames();

  if (millis() - last_tx >= 1000) {
    last_tx = millis();
    writeControlFrames();
  }

  if (state.restore_pending && state.auto_restore_enabled && state.desired_run) {
    if (millis() - state.boot_ms >= RESTORE_DELAY_MS) {
      state.restore_pending = false;
      handleRestore();
    }
  }

  delay(5);
}
