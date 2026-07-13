from __future__ import annotations

import asyncio
import time
import unittest
from argparse import Namespace

from .__main__ import _estop_observer
from .config import MQTTConfig
from .mqtt_bridge import MQTTBridge
from .sim_command import build_estop_payload, build_navigate_payload
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
    async def test_navigate_payload_requires_route_contract(self) -> None:
        nav_goal_queue: asyncio.Queue = asyncio.Queue()
        bridge = _mqtt_bridge(nav_goal_queue)

        await bridge._handle_navigate(
            {
                "order_id": "ORD-123",
                "route_id": "SIM_ONLY_V1",
                "nodes": [{"sequence": 0, "latitude": -15.0, "longitude": -47.0, "theta": "0.75"}],
                "waypoint_name": "FT_ENTRADA",
                "issued_at": int(time.time()),
            }
        )

        goal = await asyncio.wait_for(nav_goal_queue.get(), timeout=1)
        self.assertEqual(goal.order_id, "ORD-123")
        self.assertEqual(goal.waypoint_name, "FT_ENTRADA")
        self.assertEqual(goal.route_id, "SIM_ONLY_V1")
        self.assertEqual(goal.route_nodes[0].theta, 0.75)

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

    async def test_route_payload_enqueues_ordered_gps_nodes(self) -> None:
        nav_goal_queue: asyncio.Queue = asyncio.Queue()
        bridge = _mqtt_bridge(nav_goal_queue)
        await bridge._handle_navigate({
            "order_id": "SIM-ROUTE-001", "route_id": "SIM_ONLY_V1",
            "waypoint_name": "SIM_ONLY", "issued_at": int(time.time()),
            "nodes": [
                {"sequence": 0, "latitude": -15.0, "longitude": -47.0, "theta": 0.0},
                {"sequence": 1, "latitude": -15.0001, "longitude": -47.0001, "theta": 1.0},
            ],
        })
        goal = await asyncio.wait_for(nav_goal_queue.get(), timeout=1)
        self.assertEqual(goal.route_id, "SIM_ONLY_V1")
        self.assertEqual([node.sequence for node in goal.route_nodes], [0, 1])

    async def test_route_payload_rejects_out_of_order_nodes(self) -> None:
        nav_goal_queue: asyncio.Queue = asyncio.Queue()
        bridge = _mqtt_bridge(nav_goal_queue)
        await bridge._handle_navigate({
            "order_id": "SIM-ROUTE-002", "route_id": "SIM_ONLY_V1",
            "issued_at": int(time.time()),
            "nodes": [{"sequence": 1, "latitude": -15.0, "longitude": -47.0}],
        })
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

    def test_validation_helper_builds_navigate_payload_contract(self) -> None:
        payload = build_navigate_payload(
            Namespace(
                order_id="SIM-001",
                x=1.0,
                y=2.0,
                theta=0.5,
                frame="map",
                waypoint="SIM_POINT",
            )
        )

        self.assertEqual(payload["order_id"], "SIM-001")
        self.assertEqual(payload["map_frame"], "map")
        self.assertEqual(payload["pose"]["x"], 1.0)
        self.assertEqual(payload["pose"]["y"], 2.0)
        self.assertEqual(payload["pose"]["theta"], 0.5)
        self.assertEqual(payload["pose"]["frame"], "map")
        self.assertEqual(payload["waypoint_name"], "SIM_POINT")
        self.assertIsInstance(payload["issued_at"], int)

    def test_validation_helper_builds_estop_payload_contract(self) -> None:
        payload = build_estop_payload(
            Namespace(source="validation_cli", reason="operator_button")
        )

        self.assertEqual(payload["source"], "validation_cli")
        self.assertEqual(payload["reason"], "operator_button")
        self.assertIsInstance(payload["timestamp"], int)


if __name__ == "__main__":
    unittest.main()
