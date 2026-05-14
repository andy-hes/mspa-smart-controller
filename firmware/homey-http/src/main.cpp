#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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

struct SpaState {
  bool filter_on = true;
  bool heater_on = true;
  bool auto_restore_enabled = true;
  uint8_t bubbles_level = 0;
  uint8_t target_temp = 38;
  float current_temp_c = 0.0f;
  uint8_t bath_status = 0;
  bool online = false;
  uint32_t last_rx_ms = 0;
  bool restore_pending = true;
  uint32_t boot_ms = 0;
  uint32_t restore_delay_ms = 60000;
  uint8_t temp_multiplier = 1;
};

SpaState state;
AsyncWebServer server(80);

uint8_t checksum(uint8_t cmd, uint8_t value) {
  return static_cast<uint8_t>((START_BYTE + cmd + value) & 0xFF);
}

void sendFrame(uint8_t cmd, uint8_t value) {
  uint8_t frame[4] = {START_BYTE, cmd, value, checksum(cmd, value)};
  Serial2.write(frame, 4);
  Serial.printf("TX %02X %02X %02X %02X\n", frame[0], frame[1], frame[2], frame[3]);
}

void jsonError(AsyncWebServerRequest* req, int code, const char* message) {
  AsyncResponseStream* response = req->beginResponseStream("application/json");
  DynamicJsonDocument doc(256);
  doc["ok"] = false;
  doc["error"] = message;
  serializeJson(doc, *response);
  req->send(response, code);
}

void jsonStatus(AsyncWebServerRequest* req) {
  AsyncResponseStream* response = req->beginResponseStream("application/json");
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["online"] = state.online;
  doc["current_temperature_c"] = state.current_temp_c;
  doc["target_temperature_c"] = state.target_temp;
  doc["filter_on"] = state.filter_on;
  doc["heater_on"] = state.heater_on;
  doc["bubbles_level"] = state.bubbles_level;
  doc["auto_restore_enabled"] = state.auto_restore_enabled;
  doc["bath_status"] = state.bath_status;
  doc["uptime_s"] = millis() / 1000;
  serializeJson(doc, *response);
  req->send(response);
}

void ensureSafeHeaterState() {
  if (!state.filter_on) {
    state.heater_on = false;
  }
}

void handleRestore() {
  if (!state.auto_restore_enabled) return;
  if (!state.online) return;
  state.filter_on = true;
  ensureSafeHeaterState();
  state.heater_on = true;
}

void setupRoutes() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    jsonStatus(req);
  });

  server.on("/api/filter/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.filter_on = true;
    jsonStatus(req);
  });

  server.on("/api/filter/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.filter_on = false;
    ensureSafeHeaterState();
    jsonStatus(req);
  });

  server.on("/api/heater/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.filter_on = true;
    state.heater_on = true;
    jsonStatus(req);
  });

  server.on("/api/heater/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.heater_on = false;
    jsonStatus(req);
  });

  server.on("/api/bubbles/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.bubbles_level = 1;
    jsonStatus(req);
  });

  server.on("/api/bubbles/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.bubbles_level = 0;
    jsonStatus(req);
  });

  server.on("/api/target-temperature", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) return jsonError(req, 400, "missing value");
    int v = req->getParam("value", true)->value().toInt();
    if (v < 20 || v > 40) return jsonError(req, 400, "value out of range");
    state.target_temp = static_cast<uint8_t>(v);
    jsonStatus(req);
  });

  server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest* req) {
    handleRestore();
    jsonStatus(req);
  });

  server.on("/api/auto-restore/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.auto_restore_enabled = true;
    jsonStatus(req);
  });

  server.on("/api/auto-restore/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.auto_restore_enabled = false;
    jsonStatus(req);
  });

  server.onNotFound([](AsyncWebServerRequest* req) {
    jsonError(req, 404, "not found");
  });
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
            state.bath_status = value;
          }
        }
        idx = 0;
      }
    }
  }

  if (state.last_rx_ms > 0 && (millis() - state.last_rx_ms > 10000)) {
    state.online = false;
  }
}

void writeControlFrames() {
  ensureSafeHeaterState();

  sendFrame(0x02, state.filter_on ? 0x01 : 0x00);                         // filter
  sendFrame(0x01, (state.heater_on && state.filter_on) ? 0x01 : 0x00);    // heater
  sendFrame(0x03, state.bubbles_level);                                     // bubbles level
  sendFrame(0x04, static_cast<uint8_t>(state.target_temp * state.temp_multiplier));
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  state.boot_ms = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  setupRoutes();
  server.begin();
}

void loop() {
  static uint32_t last_tx = 0;

  readFrames();

  if (millis() - last_tx >= 1000) {
    last_tx = millis();
    writeControlFrames();
  }

  if (state.restore_pending && state.auto_restore_enabled) {
    if (millis() - state.boot_ms >= state.restore_delay_ms) {
      state.restore_pending = false;
      handleRestore();
    }
  }

  delay(5);
}
