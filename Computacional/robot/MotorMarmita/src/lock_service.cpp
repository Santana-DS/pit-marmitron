#include "lock_service.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "lock_display/display_manager.h"

#if __has_include("lock_secrets.h")
#include "lock_secrets.h"
#define LOCK_MQTT_CONFIGURED 1
#else
#define LOCK_MQTT_CONFIGURED 0
#endif

namespace {
constexpr uint8_t kActuatorPin = 4;
constexpr uint32_t kActuatorHoldMs = 5000;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kMqttRetryMs = 5000;
constexpr uint32_t kHeartbeatMs = 30000;
constexpr uint32_t kBootFallbackMs = 5000;

constexpr char kDisplayQrTopic[] = "robot/commands/display_qr";
constexpr char kUnlockTopic[] = "robot/commands/unlock";
constexpr char kHeartbeatTopic[] = "robot/status/heartbeat";
constexpr char kNavStatusTopic[] = "robot/nav/status";

DisplayManager display;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
char pendingOrderId[64] = "";
bool actuatorArmed = false;
uint32_t actuatorStartedAt = 0;
uint32_t lastWifiAttemptAt = 0;
uint32_t lastMqttAttemptAt = 0;
uint32_t lastHeartbeatAt = 0;
uint32_t lastDisplayTickAt = 0;
uint32_t bootStartedAt = 0;

void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  JsonDocument document;
  if (deserializeJson(document, payload, length)) {
    display.showError();
    return;
  }

  const char* orderId = document["order_id"] | "";
  if (strcmp(topic, kNavStatusTopic) == 0) {
    const char* state = document["state"] | "NAVIGATING";
    const char* destination = document["waypoint_name"] | "";
    if (strcmp(state, "FAULT") == 0 || strcmp(state, "FAILED") == 0 ||
        strcmp(state, "CANCELLED") == 0 || strcmp(state, "REJECTED") == 0) {
      display.showError();
      return;
    }
    display.showNavigation(state, destination, document["progress_pct"] | 0.0f);
    return;
  }
  if (orderId[0] == '\0' || document["issued_at"].isNull()) {
    display.showError();
    return;
  }

  if (strcmp(topic, kDisplayQrTopic) == 0) {
    const char* otp = document["otp"] | "";
    if (strlen(otp) != 4) {
      display.showError();
      return;
    }
    for (uint8_t index = 0; index < 4; ++index) {
      if (otp[index] < '0' || otp[index] > '9') {
        display.showError();
        return;
      }
    }
    strncpy(pendingOrderId, orderId, sizeof(pendingOrderId) - 1);
    pendingOrderId[sizeof(pendingOrderId) - 1] = '\0';
    display.showQrCode(otp);
    return;
  }

  if (strcmp(topic, kUnlockTopic) != 0 || pendingOrderId[0] == '\0' ||
      strcmp(pendingOrderId, orderId) != 0) {
    display.showError();
    return;
  }

  const char* code = document["code"] | "";
  if (strlen(code) != 4) {
    display.showError();
    return;
  }

  display.showUnlockSuccess(orderId);
  actuatorArmed = true;
  actuatorStartedAt = millis();
  pendingOrderId[0] = '\0';
}

void updateDisplay() {
  const uint32_t now = millis();
  const uint32_t interval = display.mode() == ScreenMode::UNLOCK_SUCCESS ? 16 : 30;
  if (now - lastDisplayTickAt < interval) return;
  lastDisplayTickAt = now;

  switch (display.mode()) {
    case ScreenMode::BOOTING: display.tickBooting(); break;
    case ScreenMode::IDLE: display.tickIdle(); break;
    case ScreenMode::UNLOCK_SUCCESS: display.tickUnlockSuccess(); break;
    default: break;
  }
}

void updateActuator() {
  if (!actuatorArmed) return;
  digitalWrite(kActuatorPin, HIGH);
  if (millis() - actuatorStartedAt >= kActuatorHoldMs) {
    digitalWrite(kActuatorPin, LOW);
    actuatorArmed = false;
    display.showIdle();
  }
}

#if LOCK_MQTT_CONFIGURED
void publishHeartbeat() {
  if (!mqtt.connected() || millis() - lastHeartbeatAt < kHeartbeatMs) return;
  lastHeartbeatAt = millis();

  JsonDocument document;
  document["source"] = "esp32";
  document["status"] = "online";
  document["uptime_s"] = millis() / 1000;
  document["rssi_dbm"] = WiFi.RSSI();
  document["free_heap"] = ESP.getFreeHeap();
  document["actuator_armed"] = actuatorArmed;
  document["display_ready"] = display.isReady();

  char payload[256];
  if (serializeJson(document, payload, sizeof(payload)) > 0) {
    mqtt.publish(kHeartbeatTopic, payload, false);
  }
}

void updateMqtt() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttemptAt >= kWifiRetryMs) {
      lastWifiAttemptAt = now;
      WiFi.begin(LOCK_WIFI_SSID, LOCK_WIFI_PASSWORD);
    }
    return;
  }

  if (!mqtt.connected()) {
    if (now - lastMqttAttemptAt < kMqttRetryMs) return;
    lastMqttAttemptAt = now;
    if (mqtt.connect(LOCK_MQTT_CLIENT_ID, LOCK_MQTT_USERNAME, LOCK_MQTT_PASSWORD,
                     kHeartbeatTopic, 1, false,
                     "{\"source\":\"esp32\",\"status\":\"offline\"}")) {
      mqtt.subscribe(kDisplayQrTopic, 1);
      mqtt.subscribe(kUnlockTopic, 1);
      mqtt.subscribe(kNavStatusTopic, 1);
      mqtt.publish(kHeartbeatTopic, "{\"source\":\"esp32\",\"status\":\"online\"}");
      display.showIdle();
    }
    return;
  }

  mqtt.loop();
  publishHeartbeat();
}
#endif
}  // namespace

void LockService::begin() {
  pinMode(kActuatorPin, OUTPUT);
  digitalWrite(kActuatorPin, LOW);

  if (display.begin()) {
    display.showBooting();
    bootStartedAt = millis();
  }

#if LOCK_MQTT_CONFIGURED
  // Preserve the existing Marmitron access point while connecting to the broker.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  mqtt.setServer(LOCK_MQTT_HOST, LOCK_MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(3);
  lastWifiAttemptAt = millis() - kWifiRetryMs;
#endif
}

void LockService::tick() {
  updateActuator();
  updateDisplay();
  if (display.mode() == ScreenMode::BOOTING &&
      millis() - bootStartedAt >= kBootFallbackMs) {
    display.showOffline();
  }
#if LOCK_MQTT_CONFIGURED
  updateMqtt();
#endif
}
