"""MQTT validation helper for ROS 2/Nav2 integration.

Run from Computacional/app:

    python -m edge_daemon.sim_command navigate --order-id SIM-001 --x 1 --y 2
    python -m edge_daemon.sim_command estop --reason operator_test
    python -m edge_daemon.sim_command listen --seconds 30

The helper uses the same MQTT environment variables as edge_daemon:
MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, MQTT_CLIENT_ID.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import math
import os
import sys
import time
from typing import Any

import aiomqtt

from .topics import Topics


def _load_dotenv() -> None:
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if not os.path.exists(env_path):
        return

    with open(env_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = value


def _required_env(key: str) -> str:
    value = os.environ.get(key)
    if not value:
        raise SystemExit(f"Missing required environment variable: {key}")
    return value


def _mqtt_options() -> dict[str, Any]:
    _load_dotenv()
    return {
        "hostname": _required_env("MQTT_HOST"),
        "port": int(os.environ.get("MQTT_PORT", "1883")),
        "username": _required_env("MQTT_USER"),
        "password": _required_env("MQTT_PASSWORD"),
        "identifier": f"{os.environ.get('MQTT_CLIENT_ID', 'edge')}-validation-cli",
    }


def build_navigate_payload(args: argparse.Namespace) -> dict[str, Any]:
    return {
        "order_id": args.order_id,
        "map_frame": args.frame,
        "pose": {
            "x": args.x,
            "y": args.y,
            "theta": args.theta,
            "frame": args.frame,
        },
        "waypoint_name": args.waypoint,
        "issued_at": int(time.time()),
    }


def build_estop_payload(args: argparse.Namespace) -> dict[str, Any]:
    return {
        "source": args.source,
        "reason": args.reason,
        "timestamp": int(time.time() * 1000),
    }

def build_route_payload(args: argparse.Namespace) -> dict[str, Any]:
    nodes = []
    for sequence, raw in enumerate(args.node):
        latitude, longitude, theta = map(float, raw.split(","))
        nodes.append({"sequence": sequence, "latitude": latitude, "longitude": longitude, "theta": theta})
    return {"order_id": args.order_id, "route_id": args.route_id,
            "waypoint_name": args.destination_point_key, "nodes": nodes,
            "issued_at": int(time.time())}


def build_demo_telemetry(args: argparse.Namespace, progress: float, state: str) -> dict[str, Any]:
    progress = max(0.0, min(progress, 100.0))
    ratio = progress / 100.0
    remaining_m = max(args.distance_m * (1.0 - ratio), 0.0)
    speed_mps = args.speed_mps if state == "NAVIGATING" else 0.0
    return {
        "source": "simulation_cli",
        "timestamp": int(time.time()),
        "pose": {
            "x": round(args.start_x + (args.end_x - args.start_x) * ratio, 3),
            "y": round(args.start_y + (args.end_y - args.start_y) * ratio, 3),
            "theta": round(args.theta, 4),
            "frame": args.frame,
        },
        "velocity": {"linear_mps": speed_mps},
        "battery": {"percent": max(args.battery_percent - ratio * 0.5, 0.0)},
        "nav_state": state,
        "active_order_id": args.order_id,
        "remaining_m": round(remaining_m, 2),
        "progress_pct": round(progress, 1),
        "eta_seconds": round(remaining_m / speed_mps, 1) if speed_mps > 0 else 0.0,
        "route_id": args.route_id,
        "route_node": 0,
        "cpu_pct": 18.0,
        "mem_pct": 36.0,
    }


def build_demo_nav_status(args: argparse.Namespace, progress: float, state: str) -> dict[str, Any]:
    return {
        "order_id": args.order_id,
        "route_id": args.route_id,
        "route_node": 0,
        "waypoint_name": args.destination,
        "state": state,
        "progress_pct": round(max(0.0, min(progress, 100.0)), 1),
        "remaining_m": round(max(args.distance_m * (1.0 - progress / 100.0), 0.0), 2),
        "issued_at": int(time.time()),
    }


async def _publish(topic: str, payload: dict[str, Any], qos: int) -> None:
    encoded = json.dumps(payload, separators=(",", ":"))
    async with aiomqtt.Client(**_mqtt_options()) as client:
        await client.publish(topic, payload=encoded, qos=qos, retain=False)
    print(f"published topic={topic} qos={qos}")
    print(json.dumps(payload, indent=2, sort_keys=True))


async def _listen(seconds: float, topics: list[str]) -> None:
    async with aiomqtt.Client(**_mqtt_options()) as client:
        for topic in topics:
            await client.subscribe(topic)
            print(f"subscribed topic={topic}")

        messages = client.messages
        deadline = time.monotonic() + seconds
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            try:
                message = await asyncio.wait_for(messages.__anext__(), timeout=remaining)
            except asyncio.TimeoutError:
                return

            payload = message.payload
            if isinstance(payload, bytes):
                payload = payload.decode("utf-8", errors="replace")
            print(f"\n[{message.topic}]")
            try:
                print(json.dumps(json.loads(payload), indent=2, sort_keys=True))
            except json.JSONDecodeError:
                print(payload)


async def _run_demo(args: argparse.Namespace) -> None:
    steps = max(1, math.ceil(args.duration_seconds / args.interval_seconds))
    async with aiomqtt.Client(**_mqtt_options()) as client:
        heartbeat = {
            "source": "simulation_cli",
            "status": "online",
            "nav_state": "NAVIGATING",
            "active_order": args.order_id,
            "mqtt_connected": True,
        }
        await client.publish(Topics.HEARTBEAT, json.dumps(heartbeat), qos=1, retain=False)

        print(f"demo started: {steps} updates for {args.destination}")
        for step in range(steps + 1):
            progress = step * 100.0 / steps
            telemetry = build_demo_telemetry(args, progress, "NAVIGATING")
            nav_status = build_demo_nav_status(args, progress, "NAVIGATING")
            await client.publish(Topics.TELEMETRY, json.dumps(telemetry), qos=0, retain=False)
            await client.publish(Topics.NAV_STATUS, json.dumps(nav_status), qos=1, retain=False)
            await asyncio.sleep(args.interval_seconds)

        telemetry = build_demo_telemetry(args, 100.0, "ARRIVED")
        nav_status = build_demo_nav_status(args, 100.0, "ARRIVED")
        await client.publish(Topics.TELEMETRY, json.dumps(telemetry), qos=0, retain=False)
        await client.publish(Topics.NAV_STATUS, json.dumps(nav_status), qos=1, retain=False)
    print("demo finished: ARRIVED published")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m edge_daemon.sim_command",
        description="Publish/observe MQTT commands for ROS 2 edge validation.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    nav = sub.add_parser("navigate", help="publish robot/commands/navigate")
    nav.add_argument("--order-id", default="SIM-001")
    nav.add_argument("--x", type=float, required=True)
    nav.add_argument("--y", type=float, required=True)
    nav.add_argument("--theta", type=float, default=0.0)
    nav.add_argument("--frame", default="map")
    nav.add_argument("--waypoint", default="SIM_POINT")
    nav.add_argument("--qos", type=int, default=1)

    route = sub.add_parser("route", help="publish ordered GPS route")
    route.add_argument("--order-id", default="SIM-ROUTE-001")
    route.add_argument("--route-id", default="SIM_ONLY_UNB_V1")
    route.add_argument("--destination-point-key", default="SIM_ONLY")
    route.add_argument("--node", action="append", required=True, help="latitude,longitude,theta; repeat in order")
    route.add_argument("--qos", type=int, default=1)

    estop = sub.add_parser("estop", help="publish robot/commands/estop")
    estop.add_argument("--reason", default="validation_test")
    estop.add_argument("--source", default="validation_cli")
    estop.add_argument("--qos", type=int, default=2)

    demo = sub.add_parser("demo", help="simulate telemetry and navigation status without ROS")
    demo.add_argument("--order-id", default="SIM-DEMO-001")
    demo.add_argument("--route-id", default="SIM_DEMO_V1")
    demo.add_argument("--destination", default="SIM_DEMO")
    demo.add_argument("--duration-seconds", type=float, default=30.0)
    demo.add_argument("--interval-seconds", type=float, default=1.0)
    demo.add_argument("--distance-m", type=float, default=20.0)
    demo.add_argument("--speed-mps", type=float, default=0.45)
    demo.add_argument("--battery-percent", type=float, default=86.0)
    demo.add_argument("--start-x", type=float, default=0.0)
    demo.add_argument("--start-y", type=float, default=0.0)
    demo.add_argument("--end-x", type=float, default=12.0)
    demo.add_argument("--end-y", type=float, default=6.0)
    demo.add_argument("--theta", type=float, default=0.0)
    demo.add_argument("--frame", default="map")

    listen = sub.add_parser("listen", help="listen to robot telemetry/status topics")
    listen.add_argument("--seconds", type=float, default=30.0)
    listen.add_argument(
        "--topic",
        action="append",
        dest="topics",
        default=None,
        help="topic to subscribe; may be provided multiple times",
    )
    return parser


async def _amain(argv: list[str]) -> None:
    args = _parser().parse_args(argv)
    if args.command == "navigate":
        await _publish(Topics.NAVIGATE, build_navigate_payload(args), args.qos)
    elif args.command == "route":
        await _publish(Topics.NAVIGATE, build_route_payload(args), args.qos)
    elif args.command == "estop":
        await _publish(Topics.ESTOP, build_estop_payload(args), args.qos)
    elif args.command == "demo":
        if args.duration_seconds <= 0 or args.interval_seconds <= 0 or args.distance_m < 0:
            raise SystemExit("duration, interval and distance must be positive")
        await _run_demo(args)
    elif args.command == "listen":
        topics = args.topics or [Topics.TELEMETRY, Topics.NAV_STATUS, Topics.HEARTBEAT]
        await _listen(args.seconds, topics)


def main() -> None:
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    asyncio.run(_amain(sys.argv[1:]))


if __name__ == "__main__":
    main()
