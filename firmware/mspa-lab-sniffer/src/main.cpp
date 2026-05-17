#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <vector>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef AP_SSID
#define AP_SSID "MSpa-Lab"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "mspalab123"
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

static constexpr uint8_t kStart = 0xA5;
static constexpr size_t kMaxLogLines = 3000;
static constexpr size_t kMaxLogChars = 120000;
static constexpr uint32_t kDefaultAutoStepMs = 12000;
static constexpr uint32_t kDefaultHoldMs = 3000;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

std::vector<String> frameLog;
String logText;
bool txEnabled = true;
bool autoTestEnabled = false;
uint32_t autoStepMs = kDefaultAutoStepMs;
uint32_t autoLastStepMs = 0;
int autoStepIndex = 0;
bool holdEnabled = true;
uint32_t holdIntervalMs = kDefaultHoldMs;
uint32_t lastHoldMs = 0;
uint32_t lastHoldTxMs = 0;
bool holdSendTarget = true;
bool holdSendHeartbeat = true;
bool singleMasterMode = true;

bool desiredFilter = false;
bool desiredHeater = false;
int desiredBubbles = 0;
int targetTempC = 38;
float currentTempC = 0.0f;
bool haveCurrentTemp = false;
bool rxLogEnabled = true;
uint8_t lastBathStatus = 0xFF;
uint8_t lastStatus12 = 0xFF;
uint8_t lastStatus18 = 0xFF;
uint8_t lastStatus1A = 0xFF;
bool lastEffectiveFilter = false;
bool lastEffectiveHeater = false;
uint8_t lastEffectiveBubbles = 0;
uint32_t ownershipConflictCount = 0;
String ownershipConflictLast = "";
uint32_t ownershipConflictLastMs = 0;
uint32_t ownershipLastCheckMs = 0;

bool profileTestEnabled = false;
uint32_t profileStepStartedMs = 0;
int profileStepIndex = 0;

struct AutoStep {
  const char* name;
  uint8_t cmd;
  uint8_t val;
};

AutoStep autoSteps[] = {
  {"idle_heartbeat", 0x16, 0x00},
  {"filter_on", 0x02, 0x01},
  {"heater_on", 0x01, 0x01},
  {"target_38", 0x04, 0x26},
  {"bubbles_on", 0x03, 0x01},
  {"bubbles_off", 0x03, 0x00},
  {"heater_off", 0x01, 0x00},
  {"filter_off", 0x02, 0x00},
  {"target_34", 0x04, 0x22},
  {"heartbeat", 0x16, 0x00},
};

constexpr int kAutoStepCount = sizeof(autoSteps) / sizeof(autoSteps[0]);

struct ProfileStep {
  const char* name;
  uint32_t holdMs;
  bool sendTarget;
  bool sendHeartbeat;
  bool filter;
  bool heater;
  int bubbles;
  int target;
  uint32_t durationMs;
};

ProfileStep profileSteps[] = {
  {"baseline_idle", 3000, true, true, false, false, 0, 38, 15000},
  {"filter_only_3s", 3000, true, true, true, false, 0, 38, 15000},
  {"filter_heater_3s", 3000, true, true, true, true, 0, 38, 15000},
  {"filter_bubbles_3s", 3000, true, true, true, false, 1, 38, 15000},
  {"filter_heater_2s", 2000, true, true, true, true, 0, 38, 15000},
  {"filter_heater_4s", 4000, true, true, true, true, 0, 38, 15000},
  {"no_heartbeat", 3000, true, false, true, true, 0, 38, 15000},
  {"no_target", 3000, false, true, true, true, 0, 38, 15000},
  {"target_34", 3000, true, true, true, true, 0, 34, 15000},
  {"target_38", 3000, true, true, true, true, 0, 38, 15000},
};

constexpr int kProfileStepCount = sizeof(profileSteps) / sizeof(profileSteps[0]);

uint8_t checksum(uint8_t cmd, uint8_t val) {
  return static_cast<uint8_t>((kStart + cmd + val) & 0xFF);
}

void appendLog(const String& line) {
  frameLog.push_back(line);
  if (frameLog.size() > kMaxLogLines) frameLog.erase(frameLog.begin());
  logText += line;
  logText += "\n";
  if (logText.length() > kMaxLogChars) {
    const int cut = logText.indexOf('\n', static_cast<int>(logText.length() - kMaxLogChars));
    if (cut > 0) {
      logText.remove(0, cut + 1);
    } else {
      logText.remove(0, logText.length() - static_cast<int>(kMaxLogChars));
    }
  }
  ws.textAll(line);
  Serial.println(line);
}

void logFrame(const char* dir, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, bool valid) {
  const char* name = "unknown";
  char note[96];
  note[0] = '\0';

  switch (b1) {
    case 0x01:
      name = "heater";
      snprintf(note, sizeof(note), "heater=%s", b2 ? "on" : "off");
      break;
    case 0x02:
      name = "filter";
      snprintf(note, sizeof(note), "filter=%s", b2 ? "on" : "off");
      break;
    case 0x03:
      name = "bubbles";
      snprintf(note, sizeof(note), "bubbles_level=%u", static_cast<unsigned>(b2));
      break;
    case 0x04:
      name = "target_temp";
      snprintf(note, sizeof(note), "target_raw=%u", static_cast<unsigned>(b2));
      break;
    case 0x06:
      name = "current_temp";
      snprintf(note, sizeof(note), "temp=%.1fC", b2 / 2.0f);
      break;
    case 0x08:
      name = "bath_status";
      snprintf(note, sizeof(note), "bath_status=0x%02X (%s)", b2, b2 == 0x03 ? "running" : (b2 == 0x00 ? "idle" : "other"));
      break;
    case 0x12:
      name = "status_12";
      snprintf(note, sizeof(note), "status_12=%u", static_cast<unsigned>(b2));
      break;
    case 0x16:
      name = "heartbeat";
      snprintf(note, sizeof(note), "heartbeat=%u", static_cast<unsigned>(b2));
      break;
    case 0x18:
      name = "status_18";
      snprintf(note, sizeof(note), "status_18=%u", static_cast<unsigned>(b2));
      break;
    case 0x1A:
      name = "status_1A";
      snprintf(note, sizeof(note), "status_1A=%u", static_cast<unsigned>(b2));
      break;
    default:
      snprintf(note, sizeof(note), "value=%u", static_cast<unsigned>(b2));
      break;
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "[%10lu ms] %s %02X %02X %02X %02X %s [%s] %s",
           millis(), dir, b0, b1, b2, b3, valid ? "ok" : "bad-chk", name, note);
  appendLog(String(buf));
}

bool parseHexByte(const String& s, uint8_t& out) {
  char* end = nullptr;
  unsigned long v = strtoul(s.c_str(), &end, 16);
  if (end == s.c_str() || *end != '\0' || v > 0xFF) return false;
  out = static_cast<uint8_t>(v);
  return true;
}

void sendFrame(uint8_t cmd, uint8_t val) {
  if (!txEnabled) {
    appendLog(String("[") + millis() + " ms] TX blocked (tx disabled)");
    return;
  }
  uint8_t frame[4] = {kStart, cmd, val, checksum(cmd, val)};
  Serial2.write(frame, 4);
  logFrame("TX", frame[0], frame[1], frame[2], frame[3], true);
}

void sendDesiredState(const char* reason) {
  if (!txEnabled) return;
  if (!holdEnabled) return;

  const bool filter = desiredFilter || desiredHeater || desiredBubbles > 0;
  const bool tempNeedsHeat = !haveCurrentTemp || (currentTempC < static_cast<float>(targetTempC));
  const bool heater = desiredHeater && filter && tempNeedsHeat;
  const uint8_t bubbles = static_cast<uint8_t>(desiredBubbles > 0 ? desiredBubbles : 0);
  lastEffectiveFilter = filter;
  lastEffectiveHeater = heater;
  lastEffectiveBubbles = bubbles;

  appendLog(String("[") + millis() + " ms] HOLD " + reason +
            " desired(filter=" + (filter ? "1" : "0") +
            ",heater=" + (desiredHeater ? "1" : "0") +
            ",bubbles=" + String(desiredBubbles) +
            ",target=" + String(targetTempC) +
            ",temp=" + String(currentTempC, 1) + ")");

  sendFrame(0x02, filter ? 0x01 : 0x00);                              // filter
  delay(80);
  sendFrame(0x01, heater ? 0x01 : 0x00);                              // heater
  delay(80);
  sendFrame(0x03, bubbles);                                            // bubbles
  delay(80);
  if (holdSendTarget) {
    sendFrame(0x04, static_cast<uint8_t>(targetTempC & 0xFF));        // target temp raw
    delay(80);
  }
  if (holdSendHeartbeat) {
    sendFrame(0x16, 0x00);                                             // heartbeat
  }
  lastHoldTxMs = millis();
}

void checkOwnershipConflict() {
  if (!singleMasterMode || !txEnabled || !holdEnabled) return;
  if (millis() - ownershipLastCheckMs < 1000) return;
  ownershipLastCheckMs = millis();
  // Wait a little after TX before judging mismatch
  if (millis() - lastHoldTxMs < 900) return;

  bool conflict = false;
  String why = "";

  // Strongest signal so far: status_12 == 1 when bubbles are active.
  if (lastStatus12 != 0xFF) {
    const bool bubblesRxOn = (lastStatus12 == 0x01);
    const bool bubblesWantedOn = (lastEffectiveBubbles > 0);
    if (bubblesRxOn != bubblesWantedOn) {
      conflict = true;
      why = String("bubbles mismatch desired=") + (bubblesWantedOn ? "on" : "off") +
            ", rx=" + (bubblesRxOn ? "on" : "off") + " (status12=" + String(lastStatus12) + ")";
    }
  }

  // Weak secondary check: if we want idle, but bath is still running.
  if (!conflict && !lastEffectiveFilter && !lastEffectiveHeater && lastEffectiveBubbles == 0 && lastBathStatus == 0x03) {
    conflict = true;
    why = "idle desired but bath_status=running (0x03)";
  }

  if (conflict) {
    ownershipConflictCount++;
    ownershipConflictLastMs = millis();
    ownershipConflictLast = why;
    // Throttle noisy repeats
    static uint32_t lastLogMs = 0;
    if (millis() - lastLogMs > 3000) {
      appendLog(String("[") + millis() + " ms] WARNING ownership conflict: " + why);
      lastLogMs = millis();
    }
  }
}

void runAutoStep(bool force) {
  if (!autoTestEnabled) return;
  if (!force && (millis() - autoLastStepMs < autoStepMs)) return;
  autoLastStepMs = millis();
  const AutoStep& s = autoSteps[autoStepIndex];
  appendLog(String("[") + millis() + " ms] STEP " + String(autoStepIndex) + " " + s.name);
  sendFrame(s.cmd, s.val);
  autoStepIndex = (autoStepIndex + 1) % kAutoStepCount;
}

void applyProfileStep(int idx) {
  if (idx < 0 || idx >= kProfileStepCount) return;
  const ProfileStep& s = profileSteps[idx];
  holdIntervalMs = s.holdMs;
  holdSendTarget = s.sendTarget;
  holdSendHeartbeat = s.sendHeartbeat;
  desiredFilter = s.filter;
  desiredHeater = s.heater;
  desiredBubbles = s.bubbles;
  targetTempC = s.target;
  appendLog(String("[") + millis() + " ms] PROFILE STEP " + String(idx) + " " + s.name +
            " holdMs=" + String(s.holdMs) +
            " target=" + (s.sendTarget ? "1" : "0") +
            " heartbeat=" + (s.sendHeartbeat ? "1" : "0"));
  sendDesiredState("profile_step");
}

void runProfileTest() {
  if (!profileTestEnabled) return;
  const ProfileStep& s = profileSteps[profileStepIndex];
  if (millis() - profileStepStartedMs < s.durationMs) return;
  profileStepIndex++;
  if (profileStepIndex >= kProfileStepCount) {
    profileTestEnabled = false;
    appendLog(String("[") + millis() + " ms] profile test COMPLETE");
    return;
  }
  profileStepStartedMs = millis();
  applyProfileStep(profileStepIndex);
}

void setupRoutes() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["wifi_connected"] = WiFi.isConnected();
    doc["tx_enabled"] = txEnabled;
    doc["auto_test_enabled"] = autoTestEnabled;
    doc["auto_step_ms"] = autoStepMs;
    doc["auto_step_index"] = autoStepIndex;
    doc["hold_enabled"] = holdEnabled;
    doc["hold_interval_ms"] = holdIntervalMs;
    doc["hold_send_target"] = holdSendTarget;
    doc["hold_send_heartbeat"] = holdSendHeartbeat;
    doc["single_master_mode"] = singleMasterMode;
    doc["desired_filter"] = desiredFilter;
    doc["desired_heater"] = desiredHeater;
    doc["desired_bubbles"] = desiredBubbles;
    doc["effective_filter"] = lastEffectiveFilter;
    doc["effective_heater"] = lastEffectiveHeater;
    doc["effective_bubbles"] = lastEffectiveBubbles;
    doc["target_temp_c"] = targetTempC;
    doc["current_temp_c"] = currentTempC;
    doc["have_current_temp"] = haveCurrentTemp;
    doc["last_bath_status"] = lastBathStatus;
    doc["last_status_12"] = lastStatus12;
    doc["last_status_18"] = lastStatus18;
    doc["last_status_1a"] = lastStatus1A;
    doc["ownership_conflict_count"] = ownershipConflictCount;
    doc["ownership_conflict_last"] = ownershipConflictLast;
    doc["ownership_conflict_last_ms"] = ownershipConflictLastMs;
    doc["rx_log_enabled"] = rxLogEnabled;
    doc["profile_test_enabled"] = profileTestEnabled;
    doc["profile_step_index"] = profileStepIndex;
    doc["uart_baud"] = UART_BAUD;
    doc["uart_rx_pin"] = UART_RX_PIN;
    doc["uart_tx_pin"] = UART_TX_PIN;
    String body;
    serializeJson(doc, body);
    req->send(200, "application/json", body);
  });

  server.on("/api/tx-enabled", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
      return;
    }
    String v = req->getParam("value", true)->value();
    txEnabled = (v == "1" || v == "true" || v == "on");
    req->send(200, "application/json", String("{\"ok\":true,\"tx_enabled\":") + (txEnabled ? "true}" : "false}"));
  });

  server.on("/api/rx-log-enabled", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
      return;
    }
    String v = req->getParam("value", true)->value();
    rxLogEnabled = (v == "1" || v == "true" || v == "on");
    appendLog(String("[") + millis() + " ms] rx logging " + (rxLogEnabled ? "ENABLED" : "DISABLED"));
    req->send(200, "application/json", String("{\"ok\":true,\"rx_log_enabled\":") + (rxLogEnabled ? "true}" : "false}"));
  });

  server.on("/api/auto-test/start", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (req->hasParam("step_ms", true)) {
      int ms = req->getParam("step_ms", true)->value().toInt();
      if (ms >= 2000 && ms <= 120000) autoStepMs = static_cast<uint32_t>(ms);
    }
    autoTestEnabled = true;
    autoStepIndex = 0;
    autoLastStepMs = 0;
    appendLog(String("[") + millis() + " ms] auto test START step_ms=" + String(autoStepMs));
    runAutoStep(true);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto-test/stop", HTTP_POST, [](AsyncWebServerRequest* req) {
    autoTestEnabled = false;
    appendLog(String("[") + millis() + " ms] auto test STOP");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto-test/step", HTTP_POST, [](AsyncWebServerRequest* req) {
    runAutoStep(true);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/profile-test/start", HTTP_POST, [](AsyncWebServerRequest* req) {
    autoTestEnabled = false;
    holdEnabled = true;
    txEnabled = true;
    rxLogEnabled = true;
    profileTestEnabled = true;
    profileStepIndex = 0;
    profileStepStartedMs = millis();
    appendLog(String("[") + millis() + " ms] profile test START");
    applyProfileStep(profileStepIndex);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/profile-test/stop", HTTP_POST, [](AsyncWebServerRequest* req) {
    profileTestEnabled = false;
    appendLog(String("[") + millis() + " ms] profile test STOP");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/send", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("cmd", true) || !req->hasParam("val", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing cmd/val\"}");
      return;
    }

    uint8_t cmd = 0, val = 0;
    if (!parseHexByte(req->getParam("cmd", true)->value(), cmd) || !parseHexByte(req->getParam("val", true)->value(), val)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"cmd/val must be hex bytes\"}");
      return;
    }
    sendFrame(cmd, val);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/preset/heater/on", HTTP_POST, [](AsyncWebServerRequest* req) { desiredHeater = true; desiredFilter = true; sendDesiredState("heater_on"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/heater/off", HTTP_POST, [](AsyncWebServerRequest* req) { desiredHeater = false; sendDesiredState("heater_off"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/filter/on", HTTP_POST, [](AsyncWebServerRequest* req) { desiredFilter = true; sendDesiredState("filter_on"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/filter/off", HTTP_POST, [](AsyncWebServerRequest* req) { desiredFilter = false; desiredHeater = false; sendDesiredState("filter_off"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/bubbles/off", HTTP_POST, [](AsyncWebServerRequest* req) { desiredBubbles = 0; sendDesiredState("bubbles_off"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/bubbles/1", HTTP_POST, [](AsyncWebServerRequest* req) { desiredBubbles = 1; sendDesiredState("bubbles_on"); req->send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/preset/heartbeat", HTTP_POST, [](AsyncWebServerRequest* req) { sendFrame(0x16, 0x00); req->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/api/preset/target", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
      return;
    }
    int t = req->getParam("value", true)->value().toInt();
    if (t < 15 || t > 40) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"target out of range (15..40)\"}");
      return;
    }
    targetTempC = t;
    sendDesiredState("target_set");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/hold-enabled", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
      return;
    }
    String v = req->getParam("value", true)->value();
    holdEnabled = (v == "1" || v == "true" || v == "on");
    appendLog(String("[") + millis() + " ms] hold " + (holdEnabled ? "ENABLED" : "DISABLED"));
    req->send(200, "application/json", String("{\"ok\":true,\"hold_enabled\":") + (holdEnabled ? "true}" : "false}"));
  });

  server.on("/api/hold-interval", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("ms", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing ms\"}");
      return;
    }
    int ms = req->getParam("ms", true)->value().toInt();
    if (ms < 1000 || ms > 30000) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"ms out of range (1000..30000)\"}");
      return;
    }
    holdIntervalMs = static_cast<uint32_t>(ms);
    appendLog(String("[") + millis() + " ms] hold interval set to " + String(holdIntervalMs) + "ms");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/single-master", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
      return;
    }
    String v = req->getParam("value", true)->value();
    singleMasterMode = (v == "1" || v == "true" || v == "on");
    appendLog(String("[") + millis() + " ms] single_master_mode " + (singleMasterMode ? "ENABLED" : "DISABLED"));
    req->send(200, "application/json", String("{\"ok\":true,\"single_master_mode\":") + (singleMasterMode ? "true}" : "false}"));
  });

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
    int limit = 150;
    if (req->hasParam("limit")) {
      limit = req->getParam("limit")->value().toInt();
    }
    if (limit < 1) limit = 1;
    if (limit > 400) limit = 400;

    const int total = static_cast<int>(frameLog.size());
    int start = total - limit;
    if (start < 0) start = 0;

    JsonDocument doc;
    doc["ok"] = true;
    doc["total_lines"] = total;
    doc["returned_lines"] = total - start;
    auto arr = doc["lines"].to<JsonArray>();
    for (int i = start; i < total; i++) arr.add(frameLog[i]);

    String body;
    serializeJson(doc, body);
    req->send(200, "application/json", body);
  });

  server.on("/api/logs.txt", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain; charset=utf-8", logText);
  });

  server.on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
    frameLog.clear();
    logText = "";
    appendLog(String("[") + millis() + " ms] log buffer cleared");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    const char* html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>MSpa Lab Sniffer</title>
  <style>
    body{font-family:Arial,sans-serif;margin:16px;background:#111;color:#eee}
    .row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}
    button,input{padding:8px;border-radius:6px;border:1px solid #444;background:#1e1e1e;color:#eee}
    button{cursor:pointer}
    #log{height:55vh;overflow:auto;background:#000;padding:8px;border-radius:8px;white-space:pre-wrap;font-family:Consolas,monospace}
  </style>
</head>
<body>
  <h2>MSpa Lab Sniffer + Command Debug</h2>
  <div id="status" style="background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:10px;margin:8px 0;font-family:Consolas,monospace;"></div>
  <div class="row">
    <button onclick="post('/api/preset/filter/on')">Filter ON</button>
    <button onclick="post('/api/preset/filter/off')">Filter OFF</button>
    <button onclick="post('/api/preset/heater/on')">Heater ON</button>
    <button onclick="post('/api/preset/heater/off')">Heater OFF</button>
    <button onclick="post('/api/preset/bubbles/1')">Bubbles ON</button>
    <button onclick="post('/api/preset/bubbles/off')">Bubbles OFF</button>
    <button onclick="post('/api/preset/heartbeat')">Heartbeat</button>
  </div>
  <div class="row">
    <input id="target" type="number" min="15" max="40" value="38"/>
    <button onclick="setTarget()">Set Temp</button>
    <input id="cmd" placeholder="cmd hex, ex 04" value="04"/>
    <input id="val" placeholder="val hex, ex 26" value="26"/>
    <button onclick="sendRaw()">Send Raw</button>
  </div>
  <div class="row">
    <button onclick="setTx(1)">TX Enable</button>
    <button onclick="setTx(0)">TX Disable (sniff only)</button>
    <button onclick="setRxLog(1)">RX log ON</button>
    <button onclick="setRxLog(0)">RX log OFF</button>
    <input id="hold_ms" type="number" min="1000" max="30000" value="3000"/>
    <button onclick="setHoldInterval()">Set Hold ms</button>
    <button onclick="setHold(1)">Hold ON</button>
    <button onclick="setHold(0)">Hold OFF</button>
    <button onclick="setSingleMaster(1)">Single Master ON</button>
    <button onclick="setSingleMaster(0)">Single Master OFF</button>
    <input id="step_ms" type="number" min="2000" max="120000" value="12000"/>
    <button onclick="startAuto()">Auto Test START</button>
    <button onclick="post('/api/auto-test/step')">Auto Test STEP</button>
    <button onclick="post('/api/auto-test/stop')">Auto Test STOP</button>
    <button onclick="post('/api/profile-test/start')">Profile Test START</button>
    <button onclick="post('/api/profile-test/stop')">Profile Test STOP</button>
    <button onclick="clearLog()">Clear View</button>
  </div>
  <div id="log"></div>
  <script>
    const log = document.getElementById('log');
    function append(line){log.textContent += line + "\n"; log.scrollTop = log.scrollHeight;}
    async function post(url, body){
      const res = await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''});
      append('[HTTP] '+url+' -> '+res.status);
    }
    function setTarget(){
      const t = document.getElementById('target').value;
      post('/api/preset/target','value='+encodeURIComponent(t));
    }
    function sendRaw(){
      const cmd = document.getElementById('cmd').value.trim();
      const val = document.getElementById('val').value.trim();
      post('/api/send','cmd='+encodeURIComponent(cmd)+'&val='+encodeURIComponent(val));
    }
    function setTx(v){ post('/api/tx-enabled','value='+v); }
    function setRxLog(v){ post('/api/rx-log-enabled','value='+v); }
    function setHold(v){ post('/api/hold-enabled','value='+v); }
    function setSingleMaster(v){ post('/api/single-master','value='+v); }
    function setHoldInterval(){
      const ms = document.getElementById('hold_ms').value;
      post('/api/hold-interval','ms='+encodeURIComponent(ms));
    }
    function startAuto(){
      const ms = document.getElementById('step_ms').value;
      post('/api/auto-test/start','step_ms='+encodeURIComponent(ms));
    }
    function clearLog(){ log.textContent=''; }
    async function refreshStatus(){
      const r = await fetch('/api/status');
      const s = await r.json();
      const temp = s.have_current_temp ? (s.current_temp_c.toFixed(1) + ' C') : 'n/a';
      const bath = (s.last_bath_status === 3) ? 'running (0x03)' : ((s.last_bath_status === 0) ? 'idle (0x00)' : ('0x' + Number(s.last_bath_status).toString(16)));
      document.getElementById('status').textContent =
        'Current temp: ' + temp +
        ' | Bath status: ' + bath +
        ' | 0x12: ' + s.last_status_12 +
        ' | 0x18: ' + s.last_status_18 +
        ' | 0x1A: ' + s.last_status_1a +
        ' | SingleMaster: ' + (s.single_master_mode ? 'ON' : 'OFF') +
        ' | ConflictCount: ' + s.ownership_conflict_count +
        ' | RX log: ' + (s.rx_log_enabled ? 'ON' : 'OFF') +
        ' | Hold: ' + (s.hold_enabled ? ('ON @ ' + s.hold_interval_ms + 'ms') : 'OFF') +
        ' | ProfileTest: ' + (s.profile_test_enabled ? ('ON step ' + s.profile_step_index) : 'OFF');
    }

    const ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws');
    ws.onmessage = (ev) => append(ev.data);
    fetch('/api/logs').then(r => r.json()).then(d => (d.lines || []).forEach(append));
    refreshStatus();
    setInterval(refreshStatus, 2000);
  </script>
</body>
</html>
)HTML";
    req->send(200, "text/html; charset=utf-8", html);
  });

  ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void*, uint8_t*, size_t) {
    if (type == WS_EVT_CONNECT) {
      client->text(String("[") + millis() + " ms] websocket connected");
    }
  });
  server.addHandler(&ws);
  server.begin();
}

void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("AP mode: %s / %s\n", AP_SSID, AP_PASSWORD);
    Serial.printf("Open: http://%s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.printf("WiFi connected. Open: http://%s\n", WiFi.localIP().toString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  setupWifi();
  setupRoutes();
  appendLog(String("[") + millis() + " ms] mspa-lab-sniffer ready");
  sendDesiredState("boot");
}

void loop() {
  int raw;
  uint8_t b;
  static uint8_t buf[4];
  static int idx = 0;

  // Non-blocking UART read so periodic hold TX is never starved by RX traffic.
  while (Serial2.available() > 0) {
    raw = Serial2.read();
    if (raw < 0) break;
    b = static_cast<uint8_t>(raw & 0xFF);
    if (b == kStart) {
      idx = 0;
      buf[idx++] = b;
      continue;
    }
    if (idx > 0 && idx < 4) {
      buf[idx++] = b;
      if (idx == 4) {
        const uint8_t chk = checksum(buf[1], buf[2]);
        const bool valid = (chk == buf[3]);
        if (valid) {
          if (buf[1] == 0x06) {
            currentTempC = buf[2] / 2.0f;
            haveCurrentTemp = true;
          } else if (buf[1] == 0x08) {
            lastBathStatus = buf[2];
          } else if (buf[1] == 0x12) {
            lastStatus12 = buf[2];
          } else if (buf[1] == 0x18) {
            lastStatus18 = buf[2];
          } else if (buf[1] == 0x1A) {
            lastStatus1A = buf[2];
          }
        }
        if (rxLogEnabled) {
          logFrame("RX", buf[0], buf[1], buf[2], buf[3], valid);
        }
        idx = 0;
      }
    }
  }

  ws.cleanupClients();
  if (holdEnabled && txEnabled && (millis() - lastHoldMs >= holdIntervalMs)) {
    lastHoldMs = millis();
    sendDesiredState("periodic");
  }
  runProfileTest();
  runAutoStep(false);
  checkOwnershipConflict();
  delay(2);
}
