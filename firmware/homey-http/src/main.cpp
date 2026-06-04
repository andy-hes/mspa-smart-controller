#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "mspa_tm1650_display.h"

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

static constexpr uint8_t START_BYTE = 0xA5;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 120000;
static constexpr uint32_t RESTORE_DELAY_MS = 60000;
static constexpr uint32_t RESTORE_GUARD_MS = 30000;
static constexpr uint32_t ONLINE_TIMEOUT_MS = 10000;
static constexpr uint32_t REMOTE_STATUS_GUARD_MS = 90000;
static constexpr uint32_t HOLD_INTERVAL_MS = 3000;
static constexpr uint32_t DISPLAY_UPDATE_MS = 500;
static constexpr uint32_t DISPLAY_BOOST_MS = 10000;
static constexpr uint32_t DISPLAY_FORCE_MS = 1500;
static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 10000;
static constexpr uint32_t MQTT_STATUS_INTERVAL_MS = 30000;
static constexpr uint32_t DEBOUNCE_MS = 35;

static constexpr char AP_SSID[] = "MSpa-Setup";
static constexpr char AP_PASSWORD[] = "mspasetup";
static constexpr int DISP_DIO_PIN = 23;
static constexpr int DISP_CLK_PIN = 22;

struct ButtonDef {
  const char* name;
  int pin;
};

struct LedDef {
  const char* name;
  int pin;
};

static ButtonDef BUTTONS[] = {
    {"MODE_JET", 13},
    {"HEATER", 32},
    {"FILTER", 33},
    {"TIMER", 26},
    {"BUBBLES", 25},
    {"TEMP_DOWN", 14},
    {"TEMP_UP", 27},
};

static LedDef LEDS[] = {
    {"LED_FILTER_ACTIVE", 21},
    {"LED_HEATER_FUNCTION", 19},
    {"LED_BUBBLES_ACTIVE", 18},
    {"LED_GENERAL_ERROR", 5},
    {"LED_HEATER_HEATING", 4},
};

static constexpr size_t BTN_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);
static constexpr size_t LED_COUNT = sizeof(LEDS) / sizeof(LEDS[0]);

struct SpaState {
  bool desired_filter_on = true;
  bool desired_heater_on = true;
  bool auto_restore_enabled = true;
  uint8_t desired_bubbles_level = 0;
  bool supports_ozone = false;
  bool supports_uvc = false;
  bool desired_ozone_on = false;
  bool desired_uvc_on = false;
  uint8_t target_temp = 38;
  float current_temp_c = 0.0f;
  bool have_current_temp = false;
  uint8_t bath_status = 0;
  uint8_t status_0e = 0;
  uint8_t status_15 = 0;
  bool online = false;
  bool ap_mode = false;
  uint32_t last_rx_ms = 0;
  bool restore_pending = true;
  uint32_t boot_ms = 0;
  uint32_t restore_guard_until_ms = 0;
  uint8_t temp_multiplier = 1;
  bool heater_call_for_heat = true;
  uint32_t display_last_interaction_ms = 0;
  int forced_display_value = 888;
  uint32_t forced_display_until_ms = 0;
};

struct MqttSettings {
  bool enabled = false;
  String host;
  uint16_t port = 1883;
  String username;
  String password;
  String base_topic = "mspa/controller";
};

SpaState state;
MqttSettings mqttSettings;
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;
String wifiSsid;
String wifiPassword;
WiFiClient mqttNetClient;
PubSubClient mqttClient(mqttNetClient);
MspaTm1650Display display;

bool btnStable[BTN_COUNT];
bool btnLastRead[BTN_COUNT];
uint32_t btnLastChangeMs[BTN_COUNT];
uint32_t lastMqttReconnectAttemptMs = 0;
uint32_t lastMqttStatusPublishMs = 0;
bool mqttStatusDirty = true;

uint8_t checksum(uint8_t cmd, uint8_t value) {
  return static_cast<uint8_t>((START_BYTE + cmd + value) & 0xFF);
}

bool anyDesiredActivity() {
  return state.desired_filter_on || state.desired_heater_on || state.desired_bubbles_level > 0;
}

bool anyDesiredOptionalActivity() {
  return (state.supports_ozone && state.desired_ozone_on) || (state.supports_uvc && state.desired_uvc_on);
}

void touchDisplay(uint32_t hold_ms = DISPLAY_FORCE_MS, int forced_value = -1) {
  state.display_last_interaction_ms = millis();
  if (forced_value >= 0) {
    state.forced_display_value = forced_value;
    state.forced_display_until_ms = millis() + hold_ms;
  }
}

void showTargetTempForDisplay(uint32_t hold_ms = DISPLAY_BOOST_MS) {
  state.display_last_interaction_ms = millis();
  state.forced_display_value = state.target_temp;
  state.forced_display_until_ms = millis() + hold_ms;
}

void ensureSafeDesiredState() {
  if (!state.desired_filter_on) {
    state.desired_heater_on = false;
  }
  if (state.desired_heater_on) {
    state.desired_filter_on = true;
  }
}

void normalizeMqttSettings() {
  mqttSettings.host.trim();
  mqttSettings.username.trim();
  mqttSettings.base_topic.trim();
  if (mqttSettings.port == 0) mqttSettings.port = 1883;
  if (mqttSettings.base_topic.length() == 0) mqttSettings.base_topic = "mspa/controller";
  while (mqttSettings.base_topic.endsWith("/")) {
    mqttSettings.base_topic.remove(mqttSettings.base_topic.length() - 1);
  }
}

String baseTopic() {
  normalizeMqttSettings();
  return mqttSettings.base_topic;
}

String mqttClientId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "mspa-" + mac;
}

void saveSpaSettings() {
  ensureSafeDesiredState();
  prefs.begin("mspa", false);
  prefs.putBool("filter", state.desired_filter_on);
  prefs.putBool("heater", state.desired_heater_on);
  prefs.putBool("auto", state.auto_restore_enabled);
  prefs.putUChar("bubbles", state.desired_bubbles_level);
  prefs.putBool("sup_oz", state.supports_ozone);
  prefs.putBool("sup_uvc", state.supports_uvc);
  prefs.putBool("ozone", state.desired_ozone_on);
  prefs.putBool("uvc", state.desired_uvc_on);
  prefs.putUChar("target", state.target_temp);
  prefs.putUChar("mult", state.temp_multiplier);
  prefs.end();
  mqttStatusDirty = true;
}

void loadSpaSettings() {
  prefs.begin("mspa", true);
  state.desired_filter_on = prefs.getBool("filter", true);
  state.desired_heater_on = prefs.getBool("heater", true);
  state.auto_restore_enabled = prefs.getBool("auto", true);
  state.desired_bubbles_level = prefs.getUChar("bubbles", 0);
  state.supports_ozone = prefs.getBool("sup_oz", false);
  state.supports_uvc = prefs.getBool("sup_uvc", false);
  state.desired_ozone_on = prefs.getBool("ozone", false);
  state.desired_uvc_on = prefs.getBool("uvc", false);
  state.target_temp = prefs.getUChar("target", 38);
  state.temp_multiplier = prefs.getUChar("mult", 1);
  prefs.end();
  ensureSafeDesiredState();
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

void saveMqttSettings() {
  normalizeMqttSettings();
  prefs.begin("mqtt", false);
  prefs.putBool("enabled", mqttSettings.enabled);
  prefs.putString("host", mqttSettings.host);
  prefs.putUShort("port", mqttSettings.port);
  prefs.putString("user", mqttSettings.username);
  prefs.putString("pass", mqttSettings.password);
  prefs.putString("topic", mqttSettings.base_topic);
  prefs.end();
}

void loadMqttSettings() {
  prefs.begin("mqtt", true);
  mqttSettings.enabled = prefs.getBool("enabled", false);
  mqttSettings.host = prefs.getString("host", "");
  mqttSettings.port = prefs.getUShort("port", 1883);
  mqttSettings.username = prefs.getString("user", "");
  mqttSettings.password = prefs.getString("pass", "");
  mqttSettings.base_topic = prefs.getString("topic", "mspa/controller");
  prefs.end();
  normalizeMqttSettings();
}

void applyStatusLeds() {
  digitalWrite(21, state.desired_filter_on ? HIGH : LOW);
  digitalWrite(19, state.desired_heater_on ? HIGH : LOW);
  digitalWrite(18, state.desired_bubbles_level > 0 ? HIGH : LOW);
  digitalWrite(4, (state.desired_heater_on && state.heater_call_for_heat) ? HIGH : LOW);
  digitalWrite(5, state.online ? LOW : HIGH);
}

void showDisplayText(const char* text) {
  display.displayText3(text);
}

void updateDisplay() {
  const bool boost = (millis() - state.display_last_interaction_ms) < DISPLAY_BOOST_MS;
  display.setBrightness(boost ? 7 : 1);

  if (millis() < state.forced_display_until_ms) {
    display.displayNumber3(state.forced_display_value);
    return;
  }

  if (state.ap_mode) {
    showDisplayText("AP ");
    return;
  }

  if (state.restore_pending) {
    showDisplayText("rSt");
    return;
  }

  if (!state.online) {
    showDisplayText("OFF");
    return;
  }

  if (!state.have_current_temp) {
    showDisplayText("Con");
    return;
  }

  int temp = static_cast<int>(roundf(state.current_temp_c * 10.0f));
  if (temp < 0) temp = 0;
  if (temp > 999) temp = 999;
  display.displayNumber3(temp, true);
}

void sendFrame(uint8_t cmd, uint8_t value) {
  const uint8_t frame[4] = {START_BYTE, cmd, value, checksum(cmd, value)};
  Serial2.write(frame, 4);
  Serial.printf("TX %02X %02X %02X %02X\n", frame[0], frame[1], frame[2], frame[3]);
}

void updateHeaterCallForHeat() {
  if (!state.desired_heater_on) {
    state.heater_call_for_heat = false;
  } else if (!state.have_current_temp) {
    state.heater_call_for_heat = true;
  } else if (state.current_temp_c <= static_cast<float>(state.target_temp - 1)) {
    state.heater_call_for_heat = true;
  } else if (state.current_temp_c >= static_cast<float>(state.target_temp + 1)) {
    state.heater_call_for_heat = false;
  }
}

void handleRestore() {
  if (!state.auto_restore_enabled) return;
  if (!anyDesiredActivity()) return;
  if (!state.online) return;

  ensureSafeDesiredState();
  state.restore_guard_until_ms = millis() + RESTORE_GUARD_MS;
  mqttStatusDirty = true;
}

void setDesiredFilter(bool enabled) {
  state.desired_filter_on = enabled;
  if (!enabled) {
    state.desired_heater_on = false;
  }
  ensureSafeDesiredState();
  saveSpaSettings();
}

void setDesiredHeater(bool enabled) {
  state.desired_heater_on = enabled;
  if (enabled) {
    state.desired_filter_on = true;
  }
  ensureSafeDesiredState();
  saveSpaSettings();
}

void setDesiredBubbles(uint8_t level) {
  if (level > 3) level = 3;
  state.desired_bubbles_level = level;
  saveSpaSettings();
}

void setDesiredOzone(bool enabled) {
  if (!state.supports_ozone) return;
  state.desired_ozone_on = enabled;
  saveSpaSettings();
}

void setDesiredUvc(bool enabled) {
  if (!state.supports_uvc) return;
  state.desired_uvc_on = enabled;
  saveSpaSettings();
}

void setTargetTemperature(uint8_t temp) {
  if (temp < 15) temp = 15;
  if (temp > 40) temp = 40;
  state.target_temp = temp;
  saveSpaSettings();
}

String jsonStringStatus() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["online"] = state.online;
  doc["current_temperature_c"] = state.current_temp_c;
  doc["target_temperature_c"] = state.target_temp;
  doc["filter_on"] = state.desired_filter_on;
  doc["heater_on"] = state.desired_heater_on;
  doc["bubbles_level"] = state.desired_bubbles_level;
  doc["auto_restore_enabled"] = state.auto_restore_enabled;
  doc["bath_status"] = state.bath_status;
  doc["status_0e"] = state.status_0e;
  doc["status_15"] = state.status_15;
  doc["heater_call_for_heat"] = state.heater_call_for_heat;
  doc["restore_pending"] = state.restore_pending;
  doc["supports_ozone"] = state.supports_ozone;
  doc["supports_uvc"] = state.supports_uvc;
  doc["ozone_on"] = state.desired_ozone_on;
  doc["uvc_on"] = state.desired_uvc_on;
  doc["uptime_s"] = millis() / 1000;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["ap_mode"] = state.ap_mode;
  doc["ip"] = state.ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["ssid"] = state.ap_mode ? String(AP_SSID) : WiFi.SSID();
  doc["mqtt_enabled"] = mqttSettings.enabled;
  doc["mqtt_connected"] = mqttClient.connected();
  doc["mqtt_host"] = mqttSettings.host;
  doc["mqtt_port"] = mqttSettings.port;
  doc["mqtt_base_topic"] = baseTopic();
  String payload;
  serializeJson(doc, payload);
  return payload;
}

void jsonStatus(AsyncWebServerRequest* req) {
  req->send(200, "application/json", jsonStringStatus());
}

void jsonError(AsyncWebServerRequest* req, int code, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  String payload;
  serializeJson(doc, payload);
  req->send(code, "application/json", payload);
}

String htmlChecked(bool checked) { return checked ? " checked" : ""; }

String htmlPage() {
  const String status = jsonStringStatus();
  const String mqttPassPlaceholder = mqttSettings.password.length() > 0 ? "(saved)" : "";

  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>MSpa Controller</title><style>";
  html += "body{font-family:Arial,sans-serif;margin:0;background:#10212b;color:#f5fbff}";
  html += "main{max-width:860px;margin:auto;padding:22px}";
  html += ".card{background:#183442;border:1px solid #2b5868;border-radius:8px;padding:16px;margin:14px 0}";
  html += "button,input{font:inherit;padding:10px;border-radius:6px;border:0;margin:4px}";
  html += "input{width:calc(100% - 28px)}button{background:#33a6c9;color:white}";
  html += "label{display:block;margin:6px 0}pre{white-space:pre-wrap}.row{display:flex;flex-wrap:wrap;gap:10px}";
  html += ".row>*{flex:1 1 220px}.small{font-size:0.9em;opacity:.8}</style></head><body><main>";
  html += "<h1>MSpa Controller</h1>";
  html += "<div class='card'><h2>Status</h2><pre id='status'>" + status + "</pre></div>";
  html += "<div class='card'><h2>Control</h2>";
  html += "<button onclick=\"post('/api/filter/on')\">Filter on</button><button onclick=\"post('/api/filter/off')\">Filter off</button>";
  html += "<button onclick=\"post('/api/heater/on')\">Heater on</button><button onclick=\"post('/api/heater/off')\">Heater off</button>";
  html += "<button onclick=\"post('/api/bubbles/on')\">Bubbles on</button><button onclick=\"post('/api/bubbles/off')\">Bubbles off</button>";
  if (state.supports_ozone) {
    html += "<button onclick=\"post('/api/ozone/on')\">Ozone on</button><button onclick=\"post('/api/ozone/off')\">Ozone off</button>";
  }
  if (state.supports_uvc) {
    html += "<button onclick=\"post('/api/uvc/on')\">UVC on</button><button onclick=\"post('/api/uvc/off')\">UVC off</button>";
  }
  html += "<button onclick=\"post('/api/restore')\">Restore</button>";
  html += "<button onclick=\"post('/api/auto-restore/on')\">Auto restore on</button><button onclick=\"post('/api/auto-restore/off')\">Auto restore off</button></div>";
  html += "<div class='card'><h2>Setpoint</h2><form method='post' action='/api/target-temperature'>";
  html += "<input name='value' type='number' min='15' max='40' step='1' value='" + String(state.target_temp) + "'>";
  html += "<button>Save setpoint</button></form></div>";
  html += "<div class='card'><h2>Wi-Fi</h2><form method='post' action='/api/wifi'>";
  html += "<input name='ssid' placeholder='SSID' value='" + wifiSsid + "'>";
  html += "<input name='password' type='password' placeholder='Password'>";
  html += "<button>Save Wi-Fi and restart</button></form></div>";
  html += "<div class='card'><h2>MQTT</h2><form method='post' action='/api/mqtt'>";
  html += "<label><input type='checkbox' name='enabled' value='1'" + htmlChecked(mqttSettings.enabled) + "> Enable MQTT</label>";
  html += "<div class='row'>";
  html += "<input name='host' placeholder='MQTT host' value='" + mqttSettings.host + "'>";
  html += "<input name='port' type='number' min='1' max='65535' placeholder='1883' value='" + String(mqttSettings.port) + "'>";
  html += "</div><div class='row'>";
  html += "<input name='username' placeholder='MQTT username' value='" + mqttSettings.username + "'>";
  html += "<input name='password' type='password' placeholder='MQTT password " + mqttPassPlaceholder + "'>";
  html += "</div>";
  html += "<input name='base_topic' placeholder='mspa/controller' value='" + baseTopic() + "'>";
  html += "<div class='small'>Commands under " + baseTopic() + "/command/... and status published under " + baseTopic() + "/status</div>";
  html += "<button>Save MQTT</button></form></div>";
  html += "<div class='card'><h2>Optional Features</h2><form method='post' action='/api/features'>";
  html += "<label><input type='checkbox' name='supports_ozone' value='1'" + htmlChecked(state.supports_ozone) + "> Enable ozone control (0x0E)</label>";
  html += "<label><input type='checkbox' name='supports_uvc' value='1'" + htmlChecked(state.supports_uvc) + "> Enable UVC control (0x15)</label>";
  html += "<div class='small'>Disabled by default for MSpa Mist. Enable only on models verified to support these commands.</div>";
  html += "<button>Save feature support</button></form></div>";
  html += "<script>async function post(u){await fetch(u,{method:'POST'});location.reload()}</script>";
  html += "</main></body></html>";
  return html;
}

String mqttTopic(const char* suffix) {
  return baseTopic() + "/" + String(suffix);
}

void mqttPublishRetained(const String& topic, const String& payload) {
  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishMqttStatus() {
  if (!mqttClient.connected()) return;

  mqttPublishRetained(mqttTopic("availability"), "online");
  mqttPublishRetained(mqttTopic("status"), jsonStringStatus());
  mqttPublishRetained(mqttTopic("state/filter"), state.desired_filter_on ? "on" : "off");
  mqttPublishRetained(mqttTopic("state/heater"), state.desired_heater_on ? "on" : "off");
  mqttPublishRetained(mqttTopic("state/bubbles_level"), String(state.desired_bubbles_level));
  mqttPublishRetained(mqttTopic("state/ozone"), state.desired_ozone_on ? "on" : "off");
  mqttPublishRetained(mqttTopic("state/uvc"), state.desired_uvc_on ? "on" : "off");
  mqttPublishRetained(mqttTopic("state/target_temperature_c"), String(state.target_temp));
  mqttPublishRetained(mqttTopic("state/current_temperature_c"), String(state.current_temp_c, 1));
  mqttPublishRetained(mqttTopic("state/online"), state.online ? "true" : "false");
  mqttPublishRetained(mqttTopic("state/auto_restore_enabled"), state.auto_restore_enabled ? "true" : "false");
  mqttStatusDirty = false;
  lastMqttStatusPublishMs = millis();
}

bool parseBoolPayload(const String& raw, bool* value_out) {
  String value = raw;
  value.toLowerCase();
  value.trim();
  if (value == "1" || value == "on" || value == "true" || value == "yes") {
    *value_out = true;
    return true;
  }
  if (value == "0" || value == "off" || value == "false" || value == "no") {
    *value_out = false;
    return true;
  }
  return false;
}

void mqttCallback(char* topic_cstr, byte* payload_bytes, unsigned int length) {
  String topic(topic_cstr);
  String payload;
  payload.reserve(length);
  for (unsigned int i = 0; i < length; i++) payload += static_cast<char>(payload_bytes[i]);
  payload.trim();

  const String prefix = baseTopic() + "/command/";
  if (!topic.startsWith(prefix)) return;
  const String command = topic.substring(prefix.length());

  bool bool_value = false;
  if (command == "filter/set") {
    if (!parseBoolPayload(payload, &bool_value)) return;
    setDesiredFilter(bool_value);
  } else if (command == "heater/set") {
    if (!parseBoolPayload(payload, &bool_value)) return;
    setDesiredHeater(bool_value);
  } else if (command == "bubbles/set") {
    const int level = payload.toInt();
    if (level < 0 || level > 3) return;
    setDesiredBubbles(static_cast<uint8_t>(level));
  } else if (command == "target_temperature/set") {
    const int value = payload.toInt();
    if (value < 15 || value > 40) return;
    setTargetTemperature(static_cast<uint8_t>(value));
  } else if (command == "ozone/set") {
    if (!parseBoolPayload(payload, &bool_value)) return;
    setDesiredOzone(bool_value);
  } else if (command == "uvc/set") {
    if (!parseBoolPayload(payload, &bool_value)) return;
    setDesiredUvc(bool_value);
  } else if (command == "auto_restore/set") {
    if (!parseBoolPayload(payload, &bool_value)) return;
    state.auto_restore_enabled = bool_value;
    saveSpaSettings();
  } else if (command == "restore") {
    handleRestore();
  } else {
    return;
  }

  mqttStatusDirty = true;
}

void disconnectMqtt() {
  if (mqttClient.connected()) {
    mqttPublishRetained(mqttTopic("availability"), "offline");
    mqttClient.disconnect();
  }
}

void configureMqttClient() {
  normalizeMqttSettings();
  mqttClient.setServer(mqttSettings.host.c_str(), mqttSettings.port);
  mqttClient.setCallback(mqttCallback);
}

void ensureMqttConnected() {
  if (!mqttSettings.enabled) {
    disconnectMqtt();
    return;
  }
  if (state.ap_mode || WiFi.status() != WL_CONNECTED || mqttSettings.host.length() == 0) {
    disconnectMqtt();
    return;
  }
  if (mqttClient.connected()) return;
  if (millis() - lastMqttReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS) return;
  lastMqttReconnectAttemptMs = millis();

  configureMqttClient();
  const String client_id = mqttClientId();
  const bool ok = mqttSettings.username.length() > 0
                      ? mqttClient.connect(client_id.c_str(), mqttSettings.username.c_str(), mqttSettings.password.c_str(),
                                           mqttTopic("availability").c_str(), 0, true, "offline")
                      : mqttClient.connect(client_id.c_str(), mqttTopic("availability").c_str(), 0, true, "offline");

  if (!ok) {
    Serial.printf("MQTT connect failed, rc=%d\n", mqttClient.state());
    return;
  }

  mqttClient.subscribe(mqttTopic("command/#").c_str());
  mqttStatusDirty = true;
  publishMqttStatus();
  Serial.printf("MQTT connected to %s:%u topic %s\n", mqttSettings.host.c_str(), mqttSettings.port, baseTopic().c_str());
}

void setupPins() {
  for (size_t i = 0; i < LED_COUNT; i++) {
    pinMode(LEDS[i].pin, OUTPUT);
    digitalWrite(LEDS[i].pin, LOW);
  }
  for (size_t i = 0; i < BTN_COUNT; i++) {
    pinMode(BUTTONS[i].pin, INPUT_PULLUP);
    const bool read_now = digitalRead(BUTTONS[i].pin) == LOW;
    btnStable[i] = read_now;
    btnLastRead[i] = read_now;
    btnLastChangeMs[i] = millis();
  }
}

void handleButtonPress(const char* name, int pin) {
  (void)pin;
  touchDisplay();

  if (strcmp(name, "MODE_JET") == 0) {
    handleRestore();
  } else if (strcmp(name, "HEATER") == 0) {
    setDesiredHeater(!state.desired_heater_on);
  } else if (strcmp(name, "FILTER") == 0) {
    setDesiredFilter(!state.desired_filter_on);
  } else if (strcmp(name, "TIMER") == 0) {
    state.auto_restore_enabled = !state.auto_restore_enabled;
    saveSpaSettings();
  } else if (strcmp(name, "BUBBLES") == 0) {
    setDesiredBubbles(state.desired_bubbles_level > 0 ? 0 : 1);
  } else if (strcmp(name, "TEMP_DOWN") == 0) {
    if (state.target_temp > 15) setTargetTemperature(state.target_temp - 1);
    showTargetTempForDisplay();
  } else if (strcmp(name, "TEMP_UP") == 0) {
    if (state.target_temp < 40) setTargetTemperature(state.target_temp + 1);
    showTargetTempForDisplay();
  }

  mqttStatusDirty = true;
}

void pollButtons() {
  const uint32_t now = millis();
  for (size_t i = 0; i < BTN_COUNT; i++) {
    const bool read_now = digitalRead(BUTTONS[i].pin) == LOW;
    if (read_now != btnLastRead[i]) {
      btnLastRead[i] = read_now;
      btnLastChangeMs[i] = now;
    }
    if ((now - btnLastChangeMs[i]) >= DEBOUNCE_MS && btnStable[i] != btnLastRead[i]) {
      btnStable[i] = btnLastRead[i];
      if (btnStable[i]) {
        Serial.printf("[BTN] %s (GPIO %d)\n", BUTTONS[i].name, BUTTONS[i].pin);
        handleButtonPress(BUTTONS[i].name, BUTTONS[i].pin);
      }
    }
  }
}

void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", htmlPage());
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    jsonStatus(req);
  });

  server.on("/api/filter/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredFilter(true);
    jsonStatus(req);
  });
  server.on("/api/filter/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredFilter(false);
    jsonStatus(req);
  });
  server.on("/api/heater/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredHeater(true);
    jsonStatus(req);
  });
  server.on("/api/heater/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredHeater(false);
    jsonStatus(req);
  });
  server.on("/api/bubbles/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredBubbles(1);
    jsonStatus(req);
  });
  server.on("/api/bubbles/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    setDesiredBubbles(0);
    jsonStatus(req);
  });
  server.on("/api/ozone/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!state.supports_ozone) return jsonError(req, 400, "ozone support disabled");
    setDesiredOzone(true);
    jsonStatus(req);
  });
  server.on("/api/ozone/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!state.supports_ozone) return jsonError(req, 400, "ozone support disabled");
    setDesiredOzone(false);
    jsonStatus(req);
  });
  server.on("/api/uvc/on", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!state.supports_uvc) return jsonError(req, 400, "uvc support disabled");
    setDesiredUvc(true);
    jsonStatus(req);
  });
  server.on("/api/uvc/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!state.supports_uvc) return jsonError(req, 400, "uvc support disabled");
    setDesiredUvc(false);
    jsonStatus(req);
  });

  server.on("/api/target-temperature", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) return jsonError(req, 400, "missing value");
    const int value = req->getParam("value", true)->value().toInt();
    if (value < 15 || value > 40) return jsonError(req, 400, "value out of range");
    setTargetTemperature(static_cast<uint8_t>(value));
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
    const String ssid = req->getParam("ssid", true)->value();
    const String password = req->hasParam("password", true) ? req->getParam("password", true)->value() : "";
    saveWifiSettings(ssid, password);
    req->send(200, "text/plain", "Wi-Fi saved. Restarting...");
    delay(500);
    ESP.restart();
  });

  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest* req) {
    mqttSettings.enabled = req->hasParam("enabled", true);
    mqttSettings.host = req->hasParam("host", true) ? req->getParam("host", true)->value() : "";
    mqttSettings.port = req->hasParam("port", true) ? static_cast<uint16_t>(req->getParam("port", true)->value().toInt()) : 1883;
    mqttSettings.username = req->hasParam("username", true) ? req->getParam("username", true)->value() : "";
    if (req->hasParam("password", true)) {
      const String password = req->getParam("password", true)->value();
      if (password.length() > 0) mqttSettings.password = password;
    }
    mqttSettings.base_topic = req->hasParam("base_topic", true) ? req->getParam("base_topic", true)->value() : mqttSettings.base_topic;
    saveMqttSettings();
    disconnectMqtt();
    configureMqttClient();
    req->send(200, "text/plain", "MQTT settings saved.");
  });
  server.on("/api/features", HTTP_POST, [](AsyncWebServerRequest* req) {
    state.supports_ozone = req->hasParam("supports_ozone", true);
    state.supports_uvc = req->hasParam("supports_uvc", true);
    if (!state.supports_ozone) state.desired_ozone_on = false;
    if (!state.supports_uvc) state.desired_uvc_on = false;
    saveSpaSettings();
    req->send(200, "text/plain", "Feature support saved.");
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
  const uint8_t previous = state.bath_status;
  state.bath_status = value;

  if (millis() < state.restore_guard_until_ms || millis() < REMOTE_STATUS_GUARD_MS) {
    return;
  }

  if (previous == 0x03 && value == 0x00 && (anyDesiredActivity() || anyDesiredOptionalActivity())) {
    Serial.println("Remote/off status detected. Clearing desired active state.");
    state.desired_filter_on = false;
    state.desired_heater_on = false;
    state.desired_bubbles_level = 0;
    state.desired_ozone_on = false;
    state.desired_uvc_on = false;
    state.auto_restore_enabled = false;
    saveSpaSettings();
  } else if (previous == 0x00 && value == 0x03 && !anyDesiredActivity() && !anyDesiredOptionalActivity()) {
    Serial.println("Remote/on status detected. Re-enabling default desired state.");
    state.desired_filter_on = true;
    state.desired_heater_on = true;
    state.auto_restore_enabled = true;
    saveSpaSettings();
  }
}

void readFrames() {
  static uint8_t buf[4];
  static uint8_t idx = 0;

  while (Serial2.available()) {
    const uint8_t b = static_cast<uint8_t>(Serial2.read());
    if (b == START_BYTE) {
      idx = 0;
      buf[idx++] = b;
      continue;
    }
    if (idx > 0 && idx < 4) {
      buf[idx++] = b;
      if (idx == 4) {
        const uint8_t chk = checksum(buf[1], buf[2]);
        if (chk == buf[3]) {
          state.last_rx_ms = millis();
          state.online = true;
          const uint8_t cmd = buf[1];
          const uint8_t value = buf[2];
          if (cmd == 0x06) {
            state.current_temp_c = value / 2.0f;
            state.have_current_temp = true;
          } else if (cmd == 0x08) {
            handleBathStatus(value);
          } else if (cmd == 0x0E) {
            state.status_0e = value;
          } else if (cmd == 0x15) {
            state.status_15 = value;
          }
          mqttStatusDirty = true;
        }
        idx = 0;
      }
    }
  }

  if (state.last_rx_ms > 0 && (millis() - state.last_rx_ms > ONLINE_TIMEOUT_MS)) {
    if (state.online) mqttStatusDirty = true;
    state.online = false;
  }
}

void writeControlFrames() {
  if (state.restore_pending) {
    sendFrame(0x16, 0x00);
    return;
  }

  ensureSafeDesiredState();
  updateHeaterCallForHeat();

  const bool filter_wanted = state.desired_filter_on || state.desired_heater_on || state.desired_bubbles_level > 0;
  const uint8_t filter_val = filter_wanted ? 0x01 : 0x00;
  const uint8_t heater_val = (state.desired_heater_on && filter_wanted && state.heater_call_for_heat) ? 0x01 : 0x00;
  const uint8_t bubble_val = state.desired_bubbles_level;
  const uint8_t ozone_val = (state.supports_ozone && state.desired_ozone_on) ? 0x01 : 0x00;
  const uint8_t uvc_val = (state.supports_uvc && state.desired_uvc_on) ? 0x01 : 0x00;

  sendFrame(0x02, filter_val);
  sendFrame(0x01, heater_val);
  sendFrame(0x03, bubble_val);
  sendFrame(0x04, static_cast<uint8_t>(state.target_temp * state.temp_multiplier));
  if (state.supports_ozone) sendFrame(0x0E, ozone_val);
  if (state.supports_uvc) sendFrame(0x15, uvc_val);
  sendFrame(0x16, 0x00);
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
  const uint32_t started = millis();
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
  display.begin(DISP_DIO_PIN, DISP_CLK_PIN);
  display.setBrightness(7);
  display.displayText3("888");

  state.boot_ms = millis();
  state.restore_guard_until_ms = state.boot_ms + RESTORE_DELAY_MS + RESTORE_GUARD_MS;
  state.display_last_interaction_ms = millis();
  state.forced_display_until_ms = millis() + 2000;

  setupPins();
  loadSpaSettings();
  loadWifiSettings();
  loadMqttSettings();
  configureMqttClient();
  connectWifiOrFallback();
  setupRoutes();
  server.begin();
  applyStatusLeds();
}

void loop() {
  static uint32_t last_tx_ms = 0;
  static uint32_t last_display_ms = 0;

  if (state.ap_mode) {
    dnsServer.processNextRequest();
  }

  pollButtons();
  readFrames();
  applyStatusLeds();

  if (millis() - last_tx_ms >= HOLD_INTERVAL_MS) {
    last_tx_ms = millis();
    writeControlFrames();
  }

  if (millis() - last_display_ms >= DISPLAY_UPDATE_MS) {
    last_display_ms = millis();
    updateDisplay();
  }

  if (state.restore_pending) {
    if (millis() - state.boot_ms >= RESTORE_DELAY_MS) {
      state.restore_pending = false;
      handleRestore();
      mqttStatusDirty = true;
    }
  }

  ensureMqttConnected();
  mqttClient.loop();
  if (mqttClient.connected() && (mqttStatusDirty || millis() - lastMqttStatusPublishMs >= MQTT_STATUS_INTERVAL_MS)) {
    publishMqttStatus();
  }

  delay(5);
}
