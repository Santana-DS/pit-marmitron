// =============================================================================
// src/main.cpp
// MARMITRON 3000 — ESP32 Main Firmware
// =============================================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "mqtt_manager.h"
#include "display_manager.h"
#include "secrets.h"

// =============================================================================
// Hardware
// =============================================================================
static constexpr uint8_t  GPIO_ACTUATOR_PIN = 4;
static constexpr uint32_t ACTUATOR_HOLD_MS  = 5000;

// =============================================================================
// MQTT topics
// =============================================================================
static constexpr char TOPIC_DISPLAY_QR[] = "robot/commands/display_qr";
static constexpr char TOPIC_UNLOCK[]     = "robot/commands/unlock";
static constexpr char TOPIC_HEARTBEAT[]  = "robot/status/heartbeat";

static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;

// =============================================================================
// MFA state
// =============================================================================
static char     _pendingOrderId[64] = "";

// =============================================================================
// Actuator state
// =============================================================================
struct ActuatorState {
    bool     armed;
    uint32_t armTimeMs;
    char     orderId[64];
};
static ActuatorState actuator = { false, 0, "" };

// =============================================================================
// Tick intervals (ms)  — all driven by millis(), no delay() in loop()
// =============================================================================
static constexpr uint32_t TICK_BOOT_MS    =  20;   // backlight breath
static constexpr uint32_t TICK_IDLE_MS    =  30;   // cart animation
static constexpr uint32_t TICK_UNLOCK_MS  =  16;   // ring expansion

static uint32_t lastTickBoot   = 0;
static uint32_t lastTickIdle   = 0;
static uint32_t lastTickUnlock = 0;
static uint32_t lastHeartbeat  = 0;

// =============================================================================
// Subsystem instances
// =============================================================================
static DisplayManager displayMgr;

// =============================================================================
// Forward declarations
// =============================================================================
void onDisplayQr(uint8_t* payload, unsigned int len);
void onUnlock(uint8_t* payload, unsigned int len);
void handleGpio();
void handleHeartbeat();

// =============================================================================
// MQTT manager
// =============================================================================
static MqttManager mqttManager(
    WIFI_SSID, WIFI_PASSWORD,
    MQTT_BROKER_IP, MQTT_BROKER_PORT,
    MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
    [](char* topic, uint8_t* payload, unsigned int len) {
        if      (strcmp(topic, TOPIC_DISPLAY_QR) == 0) onDisplayQr(payload, len);
        else if (strcmp(topic, TOPIC_UNLOCK)     == 0) onUnlock(payload, len);
        else Serial.printf("[MQTT] Unhandled topic: %s\n", topic);
    }
);

// =============================================================================
// setup()
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(400);
    Serial.println(F("\n=== MARMITRON 3000 — Firmware v4.0 (Landscape) ===\n"));

    if (!displayMgr.begin()) {
        Serial.println(F("[MAIN] Display init failed — display-less mode"));
    } else {
        displayMgr.showBooting();
    }

    pinMode(GPIO_ACTUATOR_PIN, OUTPUT);
    digitalWrite(GPIO_ACTUATOR_PIN, LOW);

    mqttManager.begin();
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    mqttManager.tick();
    handleGpio();
    handleHeartbeat();

    const uint32_t now = millis();

    // ── Display tick dispatch ──────────────────────────────────────────────────
    switch (displayMgr.mode()) {

        case ScreenMode::BOOTING:
            if (now - lastTickBoot >= TICK_BOOT_MS) {
                lastTickBoot = now;
                displayMgr.tickBooting();
            }
            break;

        case ScreenMode::IDLE:
            if (now - lastTickIdle >= TICK_IDLE_MS) {
                lastTickIdle = now;
                displayMgr.tickIdle();
            }
            break;

        case ScreenMode::UNLOCK_SUCCESS:
            if (now - lastTickUnlock >= TICK_UNLOCK_MS) {
                lastTickUnlock = now;
                displayMgr.tickUnlockSuccess();
            }
            break;

        // QR_CODE and ERROR_FAULT are fully static — no tick needed.
        default:
            break;
    }

    // ── Connection state → display state transition ────────────────────────────
    static ConnectionState lastConnState = ConnectionState::CS_BOOT;
    ConnectionState curConnState = mqttManager.state();

    if (curConnState != lastConnState) {
        lastConnState = curConnState;

        if (curConnState == ConnectionState::CS_MQTT_CONNECTED) {
            // Only transition to idle if we aren't mid-delivery.
            if (_pendingOrderId[0] == '\0' &&
                displayMgr.mode() != ScreenMode::UNLOCK_SUCCESS) {
                displayMgr.showIdle();
            }
        } else if (curConnState == ConnectionState::CS_WIFI_CONNECTING ||
                   curConnState == ConnectionState::CS_BOOT) {
            if (displayMgr.mode() != ScreenMode::BOOTING) {
                displayMgr.showBooting();
            }
        }
    }
}

// =============================================================================
// onDisplayQr()
// =============================================================================
void onDisplayQr(uint8_t* payload, unsigned int len) {
    Serial.printf("\n[DISPLAY_QR] Received (%d bytes)\n", len);

    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) {
        displayMgr.showError();
        return;
    }

    const char* orderId  = doc["order_id"];
    const char* otp      = doc["otp"];
    long        issuedAt = doc["issued_at"];

    if (!orderId || orderId[0] == '\0') { Serial.println(F("[DISPLAY_QR] Missing order_id")); return; }
    if (!otp || strlen(otp) != 4)       { displayMgr.showError(); return; }
    for (int i = 0; i < 4; i++) {
        if (otp[i] < '0' || otp[i] > '9') { displayMgr.showError(); return; }
    }
    if (issuedAt == 0) { Serial.println(F("[DISPLAY_QR] Missing issued_at")); return; }

    strncpy(_pendingOrderId, orderId, sizeof(_pendingOrderId) - 1);
    _pendingOrderId[sizeof(_pendingOrderId) - 1] = '\0';

    Serial.printf("[DISPLAY_QR] OK — order: %s  otp: %s\n", orderId, otp);
    displayMgr.showQrCode(otp);
}

// =============================================================================
// onUnlock()
// =============================================================================
void onUnlock(uint8_t* payload, unsigned int len) {
    Serial.printf("\n[UNLOCK] Received (%d bytes)\n", len);

    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;

    const char* orderId  = doc["order_id"];
    const char* code     = doc["code"];
    long        issuedAt = doc["issued_at"];

    if (!orderId || orderId[0] == '\0') { Serial.println(F("[UNLOCK] Missing order_id")); return; }
    if (!code    || strlen(code)  != 4) { Serial.println(F("[UNLOCK] Invalid code"));     return; }
    if (issuedAt == 0)                  { Serial.println(F("[UNLOCK] Missing issued_at")); return; }

    // MFA cross-validation
    if (_pendingOrderId[0] == '\0') {
        Serial.println(F("[UNLOCK] REJECTED — no pending QR"));
        displayMgr.showError();
        return;
    }
    if (strncmp(_pendingOrderId, orderId, sizeof(_pendingOrderId)) != 0) {
        Serial.println(F("[UNLOCK] REJECTED — order_id mismatch"));
        displayMgr.showError();
        return;
    }

    Serial.printf("[UNLOCK] OK — arming actuator for order '%s'\n", orderId);

    displayMgr.showUnlockSuccess(orderId);

    strncpy(actuator.orderId, orderId, sizeof(actuator.orderId) - 1);
    actuator.orderId[sizeof(actuator.orderId) - 1] = '\0';
    actuator.armed     = true;
    actuator.armTimeMs = millis();

    _pendingOrderId[0] = '\0';
}

// =============================================================================
// handleGpio()
// =============================================================================
void handleGpio() {
    if (!actuator.armed) {
        digitalWrite(GPIO_ACTUATOR_PIN, LOW);
        return;
    }

    digitalWrite(GPIO_ACTUATOR_PIN, HIGH);

    if (millis() - actuator.armTimeMs >= ACTUATOR_HOLD_MS) {
        digitalWrite(GPIO_ACTUATOR_PIN, LOW);
        actuator.armed = false;
        Serial.printf("[GPIO] Released after %lu ms (order %s)\n",
                      millis() - actuator.armTimeMs, actuator.orderId);

        if (mqttManager.isConnected()) displayMgr.showIdle();
        else                           displayMgr.showBooting();
    }
}

// =============================================================================
// handleHeartbeat()
// =============================================================================
void handleHeartbeat() {
    if (!mqttManager.isConnected()) return;
    const uint32_t now = millis();
    if (now - lastHeartbeat < HEARTBEAT_INTERVAL_MS) return;
    lastHeartbeat = now;

    JsonDocument doc;
    doc["source"]         = "esp32";
    doc["status"]         = "online";
    doc["uptime_s"]       = now / 1000;
    doc["rssi_dbm"]       = WiFi.RSSI();
    doc["free_heap"]      = ESP.getFreeHeap();
    doc["actuator_armed"] = actuator.armed;
    doc["display_ready"]  = displayMgr.isReady();
    doc["pending_order"]  = (_pendingOrderId[0] != '\0') ? _pendingOrderId : "";

    char buf[320];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) { Serial.println(F("[HB] Serialise error")); return; }

    bool ok = mqttManager.publish(TOPIC_HEARTBEAT, buf);
    Serial.printf("[HB] %s (uptime %lus heap %uB pending '%s')\n",
                  ok ? "OK" : "FAIL", now / 1000,
                  ESP.getFreeHeap(),
                  _pendingOrderId[0] ? _pendingOrderId : "-");
}