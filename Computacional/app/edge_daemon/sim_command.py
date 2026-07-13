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
    elif args.command == "listen":
        topics = args.topics or [Topics.TELEMETRY, Topics.NAV_STATUS, Topics.HEARTBEAT]
        await _listen(args.seconds, topics)


def main() -> None:
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    asyncio.run(_amain(sys.argv[1:]))


if __name__ == "__main__":
    main()
