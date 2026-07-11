# edge_daemon/telemetry.py
#
# UnBot Delivery — Telemetry Collector
# ──────────────────────────────────────
# Publishes robot/telemetry at a configurable rate and robot/nav/eta when
# a navigation goal is active. Also maintains the rolling speed average
# in the state machine by subscribing to ROS 2 /odom.
#
# ROS 2 subscribers run in a separate rclpy executor thread managed here.
# They communicate back to the asyncio loop via asyncio.Queue.

from __future__ import annotations

import asyncio
import json
import logging
import math
import platform
import time
from typing import Optional

try:
    import psutil
    PSUTIL_AVAILABLE = True
except ImportError:
    PSUTIL_AVAILABLE = False

try:
    import rclpy
    from rclpy.node import Node
    from geometry_msgs.msg import PoseWithCovarianceStamped
    from nav_msgs.msg import Odometry
    from sensor_msgs.msg import BatteryState
    import tf2_ros
    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False

from .config import TelemetryConfig, NavConfig
from .state_machine import RobotState, RobotStateMachine
from .topics import Topics


logger = logging.getLogger(__name__)


class TelemetryCollector:
    """
    Publishes telemetry and ETA over MQTT at configurable rates.
    Also subscribes to /odom to maintain the speed average in the state machine.
    """

    def __init__(
        self,
        tel_cfg: TelemetryConfig,
        nav_cfg: NavConfig,
        state: RobotStateMachine,
        mqtt_bridge,
    ) -> None:
        self._tel_cfg    = tel_cfg
        self._nav_cfg    = nav_cfg
        self._state      = state
        self._mqtt       = mqtt_bridge

        # Latest values from ROS 2 subscriptions (updated from a separate thread).
        # Simple assignments are GIL-protected; no asyncio.Lock needed here
        # because these are only read in the asyncio event loop and written
        # from a single ROS 2 executor thread.
        self._odom_speed_mps:  float = 0.0
        self._battery_pct:     float = 0.0
        self._battery_voltage: Optional[float] = None
        self._pose_x:          float = 0.0
        self._pose_y:          float = 0.0
        self._pose_theta:      float = 0.0
        self._pose_frame:      str = "unknown"
        self._has_localized_pose = False

        self._ros_node:         Optional[object] = None
        self._tf_buffer:        Optional[object] = None
        self._tf_listener:      Optional[object] = None
        self._ros_executor_thread = None

    # ── Main coroutines ───────────────────────────────────────────────────────

    async def run(self) -> None:
        """
        Launch telemetry tasks. Never returns — run as an asyncio Task.
        """
        await self._init_ros2_subscribers()

        interval_s = 1.0 / self._tel_cfg.publish_hz
        hb_interval = self._tel_cfg.heartbeat_interval
        eta_interval = self._tel_cfg.eta_interval

        last_hb  = 0.0
        last_eta = 0.0

        while True:
            now = time.monotonic()

            # Telemetry at configured Hz.
            await self._publish_telemetry()

            # Heartbeat at 30s interval.
            if now - last_hb >= hb_interval:
                await self._publish_heartbeat()
                last_hb = now

            # ETA when navigating.
            if (
                self._state.state == RobotState.NAVIGATING
                and now - last_eta >= eta_interval
            ):
                await self._publish_eta()
                last_eta = now

            await asyncio.sleep(interval_s)

    # ── Publishers ────────────────────────────────────────────────────────────

    async def _publish_telemetry(self) -> None:
        self._refresh_tf_pose()
        payload = {
            "source":    "edge_daemon",
            "timestamp": int(time.time()),
            "pose": {
                "x":     round(self._pose_x, 3),
                "y":     round(self._pose_y, 3),
                "theta": round(self._pose_theta, 4),
                "frame": self._pose_frame,
            },
            "velocity": {
                "linear_mps": round(self._odom_speed_mps, 3),
            },
            "battery": {
                "percent": round(self._battery_pct, 1),
                "voltage_v": (
                    round(self._battery_voltage, 2)
                    if self._battery_voltage is not None
                    else None
                ),
            },
            **self._state.to_telemetry_dict(),
            **self._system_stats(),
        }
        await self._mqtt.publish(Topics.TELEMETRY, payload, qos=0)

    async def _publish_heartbeat(self) -> None:
        goal = self._state.active_goal
        payload = {
            "source":        "edge_daemon",
            "status":        "online",
            "uptime_s":      int(time.monotonic()),
            "nav_state":     self._state.state.name,
            "active_order":  goal.order_id if goal else "",
            "mqtt_connected": self._mqtt.is_connected,
        }
        await self._mqtt.publish(Topics.HEARTBEAT, payload, qos=1)
        logger.debug("Heartbeat published")

    async def _publish_eta(self) -> None:
        goal = self._state.active_goal
        if goal is None:
            return
        eta_s = self._state.compute_eta_seconds(
            min_speed=self._nav_cfg.min_speed_for_eta
        )
        if eta_s is None:
            return
        payload = {
            "order_id":    goal.order_id,
            "eta_seconds": round(eta_s, 1),
            "distance_m":  round(goal.remaining_m, 2),
            "issued_at":   int(time.time()),
        }
        await self._mqtt.publish(Topics.NAV_ETA, payload, qos=0)
        logger.debug(
            "ETA published: order=%s eta=%.1fs remaining=%.2fm",
            goal.order_id, eta_s, goal.remaining_m,
        )

    # ── System stats ──────────────────────────────────────────────────────────

    def _system_stats(self) -> dict:
        if not PSUTIL_AVAILABLE:
            return {}
        return {
            "cpu_pct":  round(psutil.cpu_percent(), 1),
            "mem_pct":  round(psutil.virtual_memory().percent, 1),
        }

    # ── ROS 2 subscribers ─────────────────────────────────────────────────────

    async def _init_ros2_subscribers(self) -> None:
        if not ROS2_AVAILABLE:
            logger.warning("ROS 2 not available — telemetry will show zero pose/battery")
            return

        loop = asyncio.get_event_loop()
        await loop.run_in_executor(None, self._setup_ros2_subscribers_sync)

    def _setup_ros2_subscribers_sync(self) -> None:
        """
        Runs in a thread. Sets up ROS 2 subscribers and spins them
        in a background thread so they never block the asyncio loop.
        """
        import threading

        self._ros_node = rclpy.create_node("unbot_telemetry_collector")

        self._ros_node.create_subscription(
            Odometry,
            self._nav_cfg.odom_topic,
            self._odom_callback,
            10,
        )
        self._ros_node.create_subscription(
            BatteryState,
            self._nav_cfg.battery_topic,
            self._battery_callback,
            10,
        )
        self._tf_buffer = tf2_ros.Buffer()
        self._tf_listener = tf2_ros.TransformListener(self._tf_buffer, self._ros_node)

        if self._nav_cfg.pose_topic:
            self._ros_node.create_subscription(
                PoseWithCovarianceStamped,
                self._nav_cfg.pose_topic,
                self._pose_callback,
                10,
            )
            logger.info("Localized pose topic subscriber started on %s", self._nav_cfg.pose_topic)
        else:
            logger.info(
                "Localized pose topic disabled; deriving pose from TF %s -> %s",
                self._nav_cfg.tf_map_frame,
                self._nav_cfg.tf_base_frame,
            )

        # Spin in background thread so asyncio event loop is not blocked.
        def spin():
            rclpy.spin(self._ros_node)

        self._ros_executor_thread = threading.Thread(target=spin, daemon=True)
        self._ros_executor_thread.start()
        logger.info("ROS 2 telemetry subscribers started")

    def _odom_callback(self, msg) -> None:
        """
        ROS 2 callback — runs in the ROS executor thread.
        Only writes to simple float attributes (GIL-protected).
        """
        vx = msg.twist.twist.linear.x
        vy = msg.twist.twist.linear.y
        self._odom_speed_mps = math.sqrt(vx ** 2 + vy ** 2)

        if not self._has_localized_pose:
            self._pose_x = msg.pose.pose.position.x
            self._pose_y = msg.pose.pose.position.y
            self._pose_frame = msg.header.frame_id or "odom"

            q = msg.pose.pose.orientation
            self._pose_theta = _quat_to_yaw(q)

    def _battery_callback(self, msg) -> None:
        """
        ROS 2 battery callback. This is expected to represent the robot's
        traction/mobility battery, not the onboard notebook battery.
        """
        if msg.percentage is not None and not math.isnan(msg.percentage):
            pct = msg.percentage * 100.0 if msg.percentage <= 1.0 else msg.percentage
            self._battery_pct = max(0.0, min(100.0, pct))
        if msg.voltage is not None and not math.isnan(msg.voltage):
            self._battery_voltage = msg.voltage

    def _pose_callback(self, msg) -> None:
        """Optional ROS 2 localized pose callback configured via ROS_POSE_TOPIC."""
        self._has_localized_pose = True
        self._pose_x = msg.pose.pose.position.x
        self._pose_y = msg.pose.pose.position.y
        self._pose_frame = msg.header.frame_id or self._nav_cfg.default_map_frame
        self._pose_theta = _quat_to_yaw(msg.pose.pose.orientation)

    def _refresh_tf_pose(self) -> None:
        if not ROS2_AVAILABLE or self._ros_node is None:
            return
        tf_buffer = getattr(self, "_tf_buffer", None)
        if tf_buffer is None:
            return

        try:
            transform = tf_buffer.lookup_transform(
                self._nav_cfg.tf_map_frame,
                self._nav_cfg.tf_base_frame,
                rclpy.time.Time(),
            )
        except Exception:
            return

        translation = transform.transform.translation
        rotation = transform.transform.rotation
        self._has_localized_pose = True
        self._pose_x = translation.x
        self._pose_y = translation.y
        self._pose_frame = transform.header.frame_id or self._nav_cfg.tf_map_frame
        self._pose_theta = _quat_to_yaw(rotation)


def _quat_to_yaw(q) -> float:
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y),
        1.0 - 2.0 * (q.y ** 2 + q.z ** 2),
    )
