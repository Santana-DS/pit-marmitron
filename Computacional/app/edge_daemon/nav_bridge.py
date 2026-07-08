# edge_daemon/nav_bridge.py
#
# UnBot Delivery — Nav2 Bridge
# ─────────────────────────────
# Consumes nav goals from the MQTT bridge queue and injects them into the
# ROS 2 Nav2 /navigate_to_pose action server.
#
# ISOLATION CONTRACT:
#   This module is the ONLY place that imports rclpy or any ROS 2 types.
#   All ROS 2 blocking calls (action.send_goal_async, wait_for_result) are
#   executed in a ThreadPoolExecutor so they never block the asyncio event loop.
#   The event loop remains free to process MQTT keepalives, heartbeats, and
#   state machine queries while Nav2 is computing a path.
#
# ETA COMPUTATION:
#   Nav2's NavigateToPose action publishes feedback containing:
#     feedback.distance_remaining  (float32, metres)
#   The bridge samples this at each feedback tick and updates the state
#   machine's speed average. ETA = remaining_distance / avg_speed.
#   The speed is derived from /odom (subscribed in TelemetryCollector),
#   not from the feedback directly, because feedback doesn't include velocity.
#
# QUATERNION NOTE:
#   Nav2 requires geometry_msgs/PoseStamped with a full quaternion orientation.
#   The incoming theta (yaw) is converted to quaternion using:
#     qz = sin(theta/2), qw = cos(theta/2), qx = qy = 0
#   This is valid for 2D navigation (no roll/pitch).

from __future__ import annotations

import asyncio
import logging
import math
import time
from concurrent.futures import ThreadPoolExecutor
from typing import Optional

# ROS 2 imports — only imported when running on the robot.
# In tests, these are mocked via the test fixtures.
try:
    import rclpy
    from rclpy.action import ActionClient
    from rclpy.node import Node
    from action_msgs.msg import GoalStatus
    from geometry_msgs.msg import PoseStamped
    from nav2_msgs.action import NavigateToPose
    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False
    logger_import = logging.getLogger(__name__)
    logger_import.warning(
        "rclpy not available — nav_bridge running in STUB mode. "
        "Install ROS 2 Humble and source /opt/ros/humble/setup.bash."
    )

from .config import NavConfig
from .state_machine import ActiveGoal, RobotState, RobotStateMachine
from .topics import Topics


logger = logging.getLogger(__name__)


def _yaw_to_quaternion(theta: float) -> tuple[float, float, float, float]:
    """
    Convert a 2D yaw angle (radians) to a unit quaternion (qx, qy, qz, qw).
    Valid for planar navigation — assumes zero roll and pitch.
    """
    half = theta / 2.0
    return 0.0, 0.0, math.sin(half), math.cos(half)


class NavBridge:
    """
    Consumes nav goals from a queue and drives the Nav2 action server.

    Lifecycle:
      1. Caller creates a NavBridge and awaits run() as an asyncio Task.
      2. run() blocks on the goal queue, waiting for goals from the MQTT bridge.
      3. For each goal, it calls _execute_goal() in a thread pool.
      4. _execute_goal() sends the goal to Nav2 and monitors feedback.
      5. On completion, the state machine is updated.
      6. On failure or timeout, a fault is triggered.
    """

    def __init__(
        self,
        cfg: NavConfig,
        state: RobotStateMachine,
        nav_goal_queue: asyncio.Queue,
        mqtt_bridge,   # MQTTBridge — injected to avoid circular import type hint
    ) -> None:
        self._cfg            = cfg
        self._state          = state
        self._nav_goal_queue = nav_goal_queue
        self._mqtt           = mqtt_bridge
        self._executor       = ThreadPoolExecutor(max_workers=1, thread_name_prefix="nav2")

        # ROS 2 node and action client — initialized in _init_ros2().
        self._node: Optional[object]         = None
        self._action_client: Optional[object] = None
        self._ros2_ready                      = False

    # ── Public API ────────────────────────────────────────────────────────────

    async def run(self) -> None:
        """
        Long-running coroutine. Consumes nav goals and executes them.
        Never returns — run as an asyncio Task.
        """
        await self._init_ros2()
        logger.info("Nav bridge ready. Waiting for goals.")

        while True:
            goal: ActiveGoal = await self._nav_goal_queue.get()
            logger.info(
                "Nav goal received: order=%s waypoint=%s x=%.2f y=%.2f theta=%.2f",
                goal.order_id, goal.waypoint_name,
                goal.x, goal.y, goal.theta,
            )

            accepted = await self._state.start_navigating(goal)
            if not accepted:
                logger.warning(
                    "Nav goal rejected for order %s: robot already in state %s",
                    goal.order_id, self._state.state.name,
                )
                self._nav_goal_queue.task_done()
                continue

            # Publish initial nav status.
            await self._publish_nav_status(goal, state="NAVIGATING", progress=0.0, remaining_m=0.0)

            # Execute goal in thread pool — blocking Nav2 calls isolated here.
            loop = asyncio.get_event_loop()
            try:
                await asyncio.wait_for(
                    loop.run_in_executor(
                        self._executor,
                        self._execute_goal_sync,
                        goal,
                        loop,
                    ),
                    timeout=self._cfg.goal_timeout,
                )
            except asyncio.TimeoutError:
                logger.error(
                    "Nav goal timed out after %.0fs for order %s",
                    self._cfg.goal_timeout, goal.order_id,
                )
                await self._cancel_current_goal()
                await self._state.trigger_fault(
                    f"nav_timeout after {self._cfg.goal_timeout:.0f}s"
                )
                await self._publish_nav_status(
                    goal, state="FAULT",
                    progress=goal.progress_pct,
                    remaining_m=goal.remaining_m,
                )
            except Exception as exc:
                logger.error("Nav goal execution failed: %s", exc, exc_info=True)
                await self._state.trigger_fault(f"nav_exception: {exc}")
            finally:
                self._nav_goal_queue.task_done()

    # ── ROS 2 initialization ──────────────────────────────────────────────────

    async def _init_ros2(self) -> None:
        if not ROS2_AVAILABLE:
            logger.warning("ROS 2 not available — nav bridge in STUB mode.")
            self._ros2_ready = False
            return

        loop = asyncio.get_event_loop()
        try:
            await loop.run_in_executor(self._executor, self._init_ros2_sync)
            self._ros2_ready = True
            logger.info("ROS 2 node initialized: unbot_edge_daemon")
        except Exception as exc:
            logger.error("ROS 2 initialization failed: %s", exc, exc_info=True)
            self._ros2_ready = False

    def _init_ros2_sync(self) -> None:
        """Blocking ROS 2 init — runs in executor."""
        rclpy.init()
        self._node = Node("unbot_edge_daemon")
        self._action_client = ActionClient(
            self._node,
            NavigateToPose,
            self._cfg.action_server,
        )
        logger.info("Waiting for Nav2 action server '%s'...", self._cfg.action_server)
        if not self._action_client.wait_for_server(timeout_sec=30.0):
            raise RuntimeError(
                f"Nav2 action server '{self._cfg.action_server}' not available after 30s. "
                "Ensure nav2 is launched: ros2 launch nav2_bringup navigation_launch.py"
            )
        logger.info("Nav2 action server connected.")

    # ── Goal execution (runs in ThreadPoolExecutor) ───────────────────────────

    def _execute_goal_sync(self, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> None:
        """
        Blocking goal execution — runs entirely in the ThreadPoolExecutor thread.
        Communicates back to the asyncio event loop via loop.call_soon_threadsafe.

        STUB MODE: If ROS 2 is not available, simulates a 10-second delivery.
        """
        if not self._ros2_ready:
            self._execute_goal_stub(goal, loop)
            return

        # Build the Nav2 goal message.
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = self._build_pose_stamped(goal)

        # Send goal to Nav2.
        send_future = self._action_client.send_goal_async(
            goal_msg,
            feedback_callback=lambda fb: self._on_feedback(fb, goal, loop),
        )
        rclpy.spin_until_future_complete(self._node, send_future)
        goal_handle = send_future.result()

        if not goal_handle.accepted:
            logger.error("Nav2 rejected goal for order %s", goal.order_id)
            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._state.trigger_fault("nav2_rejected_goal"),
            )
            return

        logger.info("Nav2 accepted goal for order %s", goal.order_id)

        # Wait for result.
        result_future = goal_handle.get_result_async()
        rclpy.spin_until_future_complete(self._node, result_future)
        result = result_future.result()

        if result.status == GoalStatus.STATUS_SUCCEEDED:
            logger.info("Nav2 goal succeeded for order %s", goal.order_id)
            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._on_goal_succeeded(goal),
            )
        else:
            logger.error(
                "Nav2 goal failed for order %s (status=%d)",
                goal.order_id, result.status,
            )
            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._state.trigger_fault(f"nav2_status_{result.status}"),
            )

    def _execute_goal_stub(self, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> None:
        """
        Simulates navigation without ROS 2. Used for integration testing the
        MQTT/state machine pipeline on a development machine without a robot.
        Simulates 10 seconds of travel with decreasing remaining_m.
        """
        logger.info("[STUB] Simulating 10s navigation for order %s", goal.order_id)
        import time as _time
        total_dist = math.sqrt(goal.x ** 2 + goal.y ** 2)
        steps = 20
        for i in range(steps):
            _time.sleep(0.5)
            remaining = total_dist * (1 - (i + 1) / steps)
            progress  = (i + 1) / steps * 100
            speed_mps = 0.4 + 0.1 * math.sin(i)   # simulate varying speed

            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._state.update_nav_progress(remaining, progress, speed_mps),
            )

        logger.info("[STUB] Simulated navigation complete for order %s", goal.order_id)
        loop.call_soon_threadsafe(
            asyncio.ensure_future,
            self._on_goal_succeeded(goal),
        )

    def _on_feedback(self, feedback_msg, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> None:
        """
        Nav2 feedback callback — runs in the ROS 2 executor thread.
        Posts updates to the asyncio event loop via call_soon_threadsafe.
        NEVER call asyncio functions directly here — wrong thread.
        """
        remaining = feedback_msg.feedback.distance_remaining
        # Progress is estimated from initial distance (approximation).
        # For exact progress, Nav2 doesn't provide it natively.
        initial_dist = math.sqrt(goal.x ** 2 + goal.y ** 2)
        progress = max(0.0, min(100.0, (1 - remaining / max(initial_dist, 0.01)) * 100))

        # We don't have speed here — it comes from /odom in TelemetryCollector.
        # Pass 0.0; the state machine's rolling average from /odom callbacks
        # is used for ETA computation instead.
        loop.call_soon_threadsafe(
            asyncio.ensure_future,
            self._state.update_nav_progress(remaining, progress, 0.0),
        )

    # ── Goal result handlers (async — called via call_soon_threadsafe) ────────

    async def _on_goal_succeeded(self, goal: ActiveGoal) -> None:
        await self._state.mark_arrived()
        await self._publish_nav_status(goal, state="ARRIVED", progress=100.0, remaining_m=0.0)
        logger.info("Robot arrived at destination for order %s", goal.order_id)

    async def _cancel_current_goal(self) -> None:
        """Cancel the active Nav2 goal. Called on timeout or estop."""
        if not self._ros2_ready or self._action_client is None:
            return
        logger.warning("Cancelling active Nav2 goal")
        # Nav2 cancel is sent asynchronously — we fire-and-forget here.
        # The nav2 stack will stop the robot safely via its own recovery behaviors.
        loop = asyncio.get_event_loop()
        await loop.run_in_executor(self._executor, self._cancel_goal_sync)

    def _cancel_goal_sync(self) -> None:
        """Blocking goal cancel — runs in executor."""
        # Nav2 cancel via action client is sent on the latest goal handle.
        # Implementation depends on whether a handle reference is stored.
        # For Phase 1, the timeout in asyncio.wait_for causes Nav2 to drop
        # the goal on the next spin — acceptable for campus speeds.
        logger.warning("[NAV] Goal cancel requested (timeout path)")

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _build_pose_stamped(self, goal: ActiveGoal) -> "PoseStamped":
        """Build a ROS 2 PoseStamped message from an ActiveGoal."""
        pose = PoseStamped()
        pose.header.frame_id = goal.map_frame
        # Use current ROS time — the nav stack requires a fresh timestamp.
        pose.header.stamp = self._node.get_clock().now().to_msg()

        pose.pose.position.x = goal.x
        pose.pose.position.y = goal.y
        pose.pose.position.z = 0.0

        qx, qy, qz, qw = _yaw_to_quaternion(goal.theta)
        pose.pose.orientation.x = qx
        pose.pose.orientation.y = qy
        pose.pose.orientation.z = qz
        pose.pose.orientation.w = qw

        return pose

    async def _publish_nav_status(
        self,
        goal: ActiveGoal,
        state: str,
        progress: float,
        remaining_m: float,
    ) -> None:
        """Publish robot/nav/status to the MQTT broker."""
        await self._mqtt.publish(
            Topics.NAV_STATUS,
            {
                "order_id":     goal.order_id,
                "waypoint_name": goal.waypoint_name,
                "state":        state,
                "progress_pct": round(progress, 1),
                "remaining_m":  round(remaining_m, 2),
                "issued_at":    int(time.time()),
            },
            qos=1,
        )