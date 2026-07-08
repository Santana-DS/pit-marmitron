from __future__ import annotations

import asyncio
import time
import unittest

from .__main__ import _estop_observer
from .config import MQTTConfig
from .mqtt_bridge import MQTTBridge
from .state_machine import RobotState, RobotStateMachine


def _mqtt_bridge(nav_goal_queue: asyncio.Queue) -> MQTTBridge:
    return MQTTBridge(
        cfg=MQTTConfig(
            host="localhost",
            port=1883,
            username="test",
            password="test",
            client_id="test-edge",
        ),
        state=RobotStateMachine(),
        nav_goal_queue=nav_goal_queue,
        unlock_queue=asyncio.Queue(),
        estop_queue=asyncio.Queue(),
    )


class SafetyNavigationTests(unittest.IsolatedAsyncioTestCase):
    async def test_navigate_payload_enqueues_goal_with_pose_frame_fallback(self) -> None:
        nav_goal_queue: asyncio.Queue = asyncio.Queue()
        bridge = _mqtt_bridge(nav_goal_queue)

        await bridge._handle_navigate(
            {
                "order_id": "ORD-123",
                "pose": {"x": "1.25", "y": -2.5, "theta": "0.75", "frame": "map"},
                "waypoint_name": "FT_ENTRADA",
                "issued_at": int(time.time()),
            }
        )

        goal = await asyncio.wait_for(nav_goal_queue.get(), timeout=1)
        self.assertEqual(goal.order_id, "ORD-123")
        self.assertEqual(goal.waypoint_name, "FT_ENTRADA")
        self.assertEqual(goal.map_frame, "map")
        self.assertEqual(goal.x, 1.25)
        self.assertEqual(goal.y, -2.5)
        self.assertEqual(goal.theta, 0.75)

    async def test_navigate_payload_rejects_non_finite_pose(self) -> None:
        nav_goal_queue: asyncio.Queue = asyncio.Queue()
        bridge = _mqtt_bridge(nav_goal_queue)

        await bridge._handle_navigate(
            {
                "order_id": "ORD-123",
                "pose": {"x": "nan", "y": 1.0, "theta": 0.0},
                "issued_at": int(time.time()),
            }
        )

        self.assertTrue(nav_goal_queue.empty())

    async def test_estop_faults_robot_even_if_nav_cancel_fails(self) -> None:
        class FailingNavBridge:
            def __init__(self) -> None:
                self.cancel_calls = 0

            async def cancel_current_goal(self) -> None:
                self.cancel_calls += 1
                raise RuntimeError("nav2 cancel unavailable")

        state = RobotStateMachine()
        estop_queue: asyncio.Queue = asyncio.Queue()
        nav_bridge = FailingNavBridge()
        task = asyncio.create_task(_estop_observer(estop_queue, state, nav_bridge))

        try:
            await estop_queue.put({"reason": "operator_button"})
            await asyncio.wait_for(estop_queue.join(), timeout=1)
            self.assertEqual(nav_bridge.cancel_calls, 1)
            self.assertEqual(state.state, RobotState.FAULT)
            self.assertEqual(state.fault_reason, "estop: operator_button")
        finally:
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task


if __name__ == "__main__":
    unittest.main()
