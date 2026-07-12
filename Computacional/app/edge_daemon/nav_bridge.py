# edge_daemon/nav_bridge.py
#
# UnBot Delivery - Nav2 Bridge
#
# Consumes nav goals from the MQTT bridge queue and injects them into the
# ROS 2 Nav2 /navigate_to_pose action server.
#
# ROS 2 threading model:
#   A single MultiThreadedExecutor is created once in _init_ros2_sync() and
#   spun continuously in one dedicated background thread. Rclpy futures are
#   resolved via add_done_callback() + threading.Event, never by calling
#   spin_until_future_complete() from worker threads. This matters because
#   ESTOP can cancel a goal while another worker is waiting on the goal result.

from __future__ import annotations

import asyncio
import logging
import math
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from typing import Optional

try:
    import rclpy
    from rclpy.action import ActionClient
    from rclpy.executors import MultiThreadedExecutor
    from rclpy.node import Node
    from action_msgs.msg import GoalStatus
    from geometry_msgs.msg import PoseStamped
    from nav2_msgs.action import NavigateToPose
    from robot_localization.srv import FromLL

    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False
    logging.getLogger(__name__).warning(
        "rclpy not available - nav_bridge running in STUB mode. "
        "Install/source the robot's ROS 2 environment (Foxy or Humble)."
    )

from .config import NavConfig
from .state_machine import ActiveGoal, RobotStateMachine
from .topics import Topics


logger = logging.getLogger(__name__)


def _yaw_to_quaternion(theta: float) -> tuple[float, float, float, float]:
    half = theta / 2.0
    return 0.0, 0.0, math.sin(half), math.cos(half)


class _RclpyFutureTimeout(Exception):
    """Raised when a rclpy future does not resolve within the expected window."""


class NavBridge:
    def __init__(
        self,
        cfg: NavConfig,
        state: RobotStateMachine,
        nav_goal_queue: asyncio.Queue,
        mqtt_bridge,
    ) -> None:
        self._cfg = cfg
        self._state = state
        self._nav_goal_queue = nav_goal_queue
        self._mqtt = mqtt_bridge
        self._executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix="nav2")

        self._node: Optional[object] = None
        self._action_client: Optional[object] = None
        self._fromll_client: Optional[object] = None
        self._ros2_ready = False
        self._current_goal_handle: Optional[object] = None
        self._goal_lock = threading.Lock()
        self._cancel_requested = threading.Event()

        self._ros_executor: Optional[object] = None
        self._spin_thread: Optional[threading.Thread] = None

    async def run(self) -> None:
        await self._init_ros2()
        logger.info("Nav bridge ready. Waiting for goals.")

        while True:
            goal: ActiveGoal = await self._nav_goal_queue.get()
            logger.info(
                "Nav goal received: order=%s waypoint=%s x=%.2f y=%.2f theta=%.2f",
                goal.order_id,
                goal.waypoint_name,
                goal.x,
                goal.y,
                goal.theta,
            )

            accepted = await self._state.start_navigating(goal)
            if not accepted:
                logger.warning(
                    "Nav goal rejected for order %s: robot already in state %s",
                    goal.order_id,
                    self._state.state.name,
                )
                self._nav_goal_queue.task_done()
                continue

            await self._publish_nav_status(
                goal, state="NAVIGATING", progress=0.0, remaining_m=0.0
            )

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
                    self._cfg.goal_timeout,
                    goal.order_id,
                )
                await self._cancel_current_goal()
                await self._state.trigger_fault(
                    f"nav_timeout after {self._cfg.goal_timeout:.0f}s"
                )
                await self._publish_nav_status(
                    goal,
                    state="FAULT",
                    progress=goal.progress_pct,
                    remaining_m=goal.remaining_m,
                )
            except Exception as exc:
                logger.error("Nav goal execution failed: %s", exc, exc_info=True)
                await self._state.trigger_fault(f"nav_exception: {exc}")
            finally:
                self._nav_goal_queue.task_done()

    async def _init_ros2(self) -> None:
        if not ROS2_AVAILABLE:
            logger.warning("ROS 2 not available - nav bridge in STUB mode.")
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
        self._node = Node("unbot_edge_daemon")
        self._action_client = ActionClient(
            self._node,
            NavigateToPose,
            self._cfg.action_server,
        )
        self._fromll_client = self._node.create_client(FromLL, "/fromLL")

        self._ros_executor = MultiThreadedExecutor(num_threads=2)
        self._ros_executor.add_node(self._node)
        self._spin_thread = threading.Thread(
            target=self._ros_executor.spin,
            daemon=True,
            name="nav2-ros-spin",
        )
        self._spin_thread.start()

        logger.info("Waiting for Nav2 action server '%s'...", self._cfg.action_server)
        if not self._action_client.wait_for_server(timeout_sec=30.0):
            raise RuntimeError(
                f"Nav2 action server '{self._cfg.action_server}' not available after 30s. "
                "Ensure nav2 is launched: ros2 launch nav2_bringup navigation_launch.py"
            )
        logger.info("Nav2 action server connected.")
        if not self._fromll_client.wait_for_service(timeout_sec=30.0):
            raise RuntimeError("navsat_transform service '/fromLL' unavailable")

    def _wait_for_rclpy_future(self, future, timeout_sec: Optional[float] = None):
        done_event = threading.Event()

        def _on_done(_future) -> None:
            done_event.set()

        future.add_done_callback(_on_done)
        if not done_event.wait(timeout=timeout_sec):
            raise _RclpyFutureTimeout(
                f"rclpy future did not complete within {timeout_sec}s"
            )
        return future.result()

    def _execute_goal_sync(self, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> None:
        if not self._ros2_ready:
            self._execute_goal_stub(goal, loop)
            return

        self._cancel_requested.clear()

        for node in goal.route_nodes:
            if self._cancel_requested.is_set():
                return
            local = self._from_ll_sync(node.latitude, node.longitude)
            goal.x, goal.y, goal.theta = local.x, local.y, node.theta
            goal.current_node = node.sequence
            if not self._execute_single_goal_sync(goal, loop):
                return

        loop.call_soon_threadsafe(asyncio.ensure_future, self._on_goal_succeeded(goal))

    def _from_ll_sync(self, latitude: float, longitude: float):
        request = FromLL.Request()
        request.ll_point.latitude = latitude
        request.ll_point.longitude = longitude
        request.ll_point.altitude = 0.0
        response = self._wait_for_rclpy_future(
            self._fromll_client.call_async(request), timeout_sec=5.0
        )
        return response.map_point

    def _execute_single_goal_sync(self, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> bool:

        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = self._build_pose_stamped(goal)

        send_future = self._action_client.send_goal_async(
            goal_msg,
            feedback_callback=lambda fb: self._on_feedback(fb, goal, loop),
        )
        goal_handle = self._wait_for_rclpy_future(send_future)

        if not goal_handle.accepted:
            logger.error("Nav2 rejected goal for order %s", goal.order_id)
            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._state.trigger_fault("nav2_rejected_goal"),
            )
            return False

        with self._goal_lock:
            self._current_goal_handle = goal_handle

        logger.info("Nav2 accepted goal for order %s", goal.order_id)

        try:
            result_future = goal_handle.get_result_async()
            result = self._wait_for_rclpy_future(result_future)

            if result.status == GoalStatus.STATUS_SUCCEEDED:
                logger.info("Nav2 node %d succeeded for order %s", goal.current_node, goal.order_id)
                return True
            else:
                logger.error(
                    "Nav2 goal failed for order %s (status=%d)",
                    goal.order_id,
                    result.status,
                )
                loop.call_soon_threadsafe(
                    asyncio.ensure_future,
                    self._state.trigger_fault(f"nav2_status_{result.status}"),
                )
                return False
        finally:
            with self._goal_lock:
                if self._current_goal_handle is goal_handle:
                    self._current_goal_handle = None

    def _execute_goal_stub(self, goal: ActiveGoal, loop: asyncio.AbstractEventLoop) -> None:
        logger.info("[STUB] Simulating 10s navigation for order %s", goal.order_id)
        import time as _time

        self._cancel_requested.clear()
        total_dist = math.sqrt(goal.x**2 + goal.y**2)
        steps = 20
        for i in range(steps):
            if self._cancel_requested.is_set():
                logger.warning("[STUB] Navigation cancelled for order %s", goal.order_id)
                return

            _time.sleep(0.5)
            remaining = total_dist * (1 - (i + 1) / steps)
            progress = (i + 1) / steps * 100
            speed_mps = 0.4 + 0.1 * math.sin(i)

            loop.call_soon_threadsafe(
                asyncio.ensure_future,
                self._state.update_nav_progress(remaining, progress, speed_mps),
            )

        logger.info("[STUB] Simulated navigation complete for order %s", goal.order_id)
        loop.call_soon_threadsafe(
            asyncio.ensure_future,
            self._on_goal_succeeded(goal),
        )

    def _on_feedback(
        self,
        feedback_msg,
        goal: ActiveGoal,
        loop: asyncio.AbstractEventLoop,
    ) -> None:
        remaining = feedback_msg.feedback.distance_remaining
        initial_dist = math.sqrt(goal.x**2 + goal.y**2)
        progress = max(0.0, min(100.0, (1 - remaining / max(initial_dist, 0.01)) * 100))

        loop.call_soon_threadsafe(
            asyncio.ensure_future,
            self._state.update_nav_progress(remaining, progress, 0.0),
        )

    async def _on_goal_succeeded(self, goal: ActiveGoal) -> None:
        await self._state.mark_arrived()
        await self._publish_nav_status(
            goal, state="ARRIVED", progress=100.0, remaining_m=0.0
        )
        logger.info("Robot arrived at destination for order %s", goal.order_id)

    async def cancel_current_goal(self) -> None:
        await self._cancel_current_goal()

    async def _cancel_current_goal(self) -> None:
        self._cancel_requested.set()
        if not self._ros2_ready or self._action_client is None:
            return

        logger.warning("Cancelling active Nav2 goal")
        loop = asyncio.get_event_loop()
        await loop.run_in_executor(self._executor, self._cancel_goal_sync)

    def _cancel_goal_sync(self) -> None:
        with self._goal_lock:
            goal_handle = self._current_goal_handle

        if goal_handle is None:
            logger.warning("[NAV] Goal cancel requested, but no active goal handle exists")
            return

        cancel_future = goal_handle.cancel_goal_async()
        try:
            cancel_response = self._wait_for_rclpy_future(cancel_future, timeout_sec=5.0)
        except _RclpyFutureTimeout:
            logger.error("[NAV] Goal cancel timed out")
            return

        logger.warning(
            "[NAV] Goal cancel accepted for %d goal(s)",
            len(getattr(cancel_response, "goals_canceling", [])),
        )

    def close(self) -> None:
        if self._ros_executor is not None:
            self._ros_executor.shutdown()
        if self._spin_thread is not None:
            self._spin_thread.join(timeout=2.0)
        if self._node is not None:
            self._node.destroy_node()
        self._executor.shutdown(wait=False, cancel_futures=True)

    def _build_pose_stamped(self, goal: ActiveGoal) -> "PoseStamped":
        pose = PoseStamped()
        pose.header.frame_id = goal.map_frame
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
        await self._mqtt.publish(
            Topics.NAV_STATUS,
            {
                "order_id": goal.order_id,
                "route_id": goal.route_id,
                "route_node": goal.current_node,
                "waypoint_name": goal.waypoint_name,
                "state": state,
                "progress_pct": round(progress, 1),
                "remaining_m": round(remaining_m, 2),
                "issued_at": int(time.time()),
            },
            qos=1,
        )
