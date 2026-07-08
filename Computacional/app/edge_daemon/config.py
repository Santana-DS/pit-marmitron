# edge_daemon/config.py
#
# UnBot Delivery — Edge Gateway Daemon Configuration
# ────────────────────────────────────────────────────
# All runtime configuration comes from environment variables (loaded by
# systemd's EnvironmentFile or a local .env file during development).
# No secrets ever live in this file.
#
# USAGE:
#   Copy edge_daemon/.env.example to edge_daemon/.env and fill in values.
#   The systemd unit file sources this via EnvironmentFile=.
#
# ENVIRONMENT VARIABLES:
#   MQTT_HOST           Mosquitto broker IP (EC2 public IP)
#   MQTT_PORT           Default 1883
#   MQTT_USER           M2M username for the notebook client
#   MQTT_PASSWORD       M2M password
#   MQTT_CLIENT_ID      Unique per device, e.g. "unbot-edge-notebook-01"
#   ROS_DOMAIN_ID       ROS 2 domain, default 0
#   NAV_GOAL_TIMEOUT    Seconds before a nav goal is considered failed, default 120
#   TELEMETRY_HZ        Telemetry publish rate in Hz, default 2
#   LOG_LEVEL           DEBUG | INFO | WARNING | ERROR, default INFO

from __future__ import annotations

import os
import math
from dataclasses import dataclass, field


def _require(key: str) -> str:
    val = os.environ.get(key)
    if not val:
        raise RuntimeError(
            f"Missing required environment variable: {key}\n"
            f"Copy edge_daemon/.env.example to edge_daemon/.env and fill in values."
        )
    return val


def _optional(key: str, default: str) -> str:
    return os.environ.get(key, default)


@dataclass(frozen=True)
class MQTTConfig:
    host: str
    port: int
    username: str
    password: str
    client_id: str

    # QoS levels — match the protocol contract in docs/PROTOCOL.md
    qos_commands: int = 1        # navigate, unlock, display_qr
    qos_estop: int = 2           # emergency stop — exactly-once
    qos_telemetry: int = 0       # high-frequency positional data
    qos_status: int = 1          # heartbeat, nav status

    # Reconnection backoff (seconds)
    reconnect_min_delay: float = 2.0
    reconnect_max_delay: float = 60.0

    # Keep-alive interval (seconds) — must be < broker's max_keepalive (60s)
    keepalive: int = 30


@dataclass(frozen=True)
class NavConfig:
    # Seconds before a navigation goal is declared failed.
    # Set to estimated_eta * 2.5 to allow for detours around obstacles.
    goal_timeout: float = 120.0

    # ROS 2 action server name for Nav2 navigate_to_pose.
    action_server: str = "navigate_to_pose"

    # TF frame used by the SLAM map. Must match the map_server config.
    default_map_frame: str = "map"

    # Odometry topic — used for ETA speed averaging.
    odom_topic: str = "/odom"

    # Battery topic published by the robot's BMS or a ROS driver.
    battery_topic: str = "/battery_state"

    # cmd_vel topic — monitored for speed telemetry.
    cmd_vel_topic: str = "/cmd_vel"

    # /amcl_pose topic — live localization.
    pose_topic: str = "/amcl_pose"

    # Rolling window for ETA speed average (number of /odom samples).
    speed_avg_window: int = 10

    # Minimum speed (m/s) to avoid division-by-zero in ETA calculation.
    min_speed_for_eta: float = 0.05


@dataclass(frozen=True)
class TelemetryConfig:
    # How often to publish robot/telemetry (Hz).
    # QoS 0 so this never backs up the broker queue.
    publish_hz: float = 2.0

    # How often to publish robot/status/heartbeat (seconds).
    heartbeat_interval: float = 30.0

    # How often to publish robot/nav/eta (seconds).
    # Only published when a nav goal is active.
    eta_interval: float = 5.0


@dataclass(frozen=True)
class FaultConfig:
    # How long (seconds) the daemon waits for MQTT reconnection before
    # triggering a local safe-stop via the Nav2 cancel action.
    # A short delay prevents false positives on brief signal drops.
    offline_safe_stop_timeout: float = 300.0

    # Whether to cancel the active Nav2 goal on MQTT reconnection failure.
    # Set to False during controlled testing to avoid stopping a running demo.
    cancel_goal_on_offline_timeout: bool = True

    # Path where the active goal is persisted across daemon restarts.
    goal_persistence_path: str = "/var/lib/marmitron/active_goal.json"


@dataclass(frozen=True)
class DaemonConfig:
    mqtt: MQTTConfig
    nav: NavConfig
    telemetry: TelemetryConfig
    fault: FaultConfig
    log_level: str = "INFO"


def load_config() -> DaemonConfig:
    """
    Load configuration from environment variables.

    Call once at startup. Raises RuntimeError on missing required variables.
    """
    # Load .env file if present (development convenience only).
    # In production, systemd's EnvironmentFile handles this.
    _load_dotenv()

    return DaemonConfig(
        mqtt=MQTTConfig(
            host=_require("MQTT_HOST"),
            port=int(_optional("MQTT_PORT", "1883")),
            username=_require("MQTT_USER"),
            password=_require("MQTT_PASSWORD"),
            client_id=_optional("MQTT_CLIENT_ID", "unbot-edge-notebook-01"),
        ),
        nav=NavConfig(
            goal_timeout=float(_optional("NAV_GOAL_TIMEOUT", "120")),
        ),
        telemetry=TelemetryConfig(
            publish_hz=float(_optional("TELEMETRY_HZ", "2")),
        ),
        fault=FaultConfig(),
        log_level=_optional("LOG_LEVEL", "INFO"),
    )


def _load_dotenv() -> None:
    """
    Minimal dotenv loader — no dependency on python-dotenv.
    Reads KEY=VALUE pairs from edge_daemon/.env into os.environ.
    Existing environment variables are NOT overridden (systemd wins).
    """
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if not os.path.exists(env_path):
        return
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = value