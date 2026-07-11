# edge_daemon/topics.py
#
# Single source of truth for all MQTT topic strings on the edge side.
# Must stay in sync with:
#   gateway/internal/services/otp.go (TopicNavigate, TopicUnlock, etc.)
#   hardware/esp32-lock/src/main.cpp (TOPIC_*)

class Topics:
    # ── Cloud → Robot ─────────────────────────────────────────────────────────
    NAVIGATE    = "robot/commands/navigate"
    UNLOCK      = "robot/commands/unlock"
    DISPLAY_QR  = "robot/commands/display_qr"
    ESTOP       = "robot/commands/estop"

    # ── Robot → Cloud ─────────────────────────────────────────────────────────
    TELEMETRY   = "robot/telemetry"
    HEARTBEAT   = "robot/status/heartbeat"
    NAV_STATUS  = "robot/nav/status"
    NAV_ETA     = "robot/nav/eta"