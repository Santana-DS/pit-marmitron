# edge_daemon/mqtt_bridge.py
#
# UnBot Delivery — Edge MQTT Bridge
# ───────────────────────────────────
# Owns the MQTT connection from the notebook to the Mosquitto broker on EC2.
# All subscriptions, publications, and reconnection logic live here.
#
# ISOLATION CONTRACT:
#   The MQTT bridge communicates with the rest of the daemon ONLY through
#   asyncio.Queue objects (nav_goal_queue, unlock_queue).
#   It never calls into the nav bridge or state machine directly — this
#   prevents any blocking operation in the nav stack from holding up MQTT
#   keepalive packets, which would cause the broker to fire the LWT.
#
# THREADING MODEL:
#   aiomqtt (async MQTT client) runs inside the asyncio event loop.
#   No background threads are used for MQTT I/O.
#   The reconnection loop is a simple async while-loop with exponential backoff.

from __future__ import annotations

import asyncio
import json
import logging
import math
import time
from typing import Optional

import aiomqtt

from .config import MQTTConfig
from .state_machine import ActiveGoal, RobotState, RobotStateMachine
from .topics import Topics


logger = logging.getLogger(__name__)


class MQTTBridge:
    """
    Manages the edge-to-cloud MQTT connection.

    Provides:
      - connect_loop(): long-running coroutine that maintains the connection
        with exponential backoff reconnection.
      - publish(): non-blocking publish (drops message if disconnected rather
        than blocking the event loop — callers must handle this gracefully
        for telemetry; critical commands retry on the caller side).
    """

    def __init__(
        self,
        cfg: MQTTConfig,
        state: RobotStateMachine,
        nav_goal_queue: asyncio.Queue,
        unlock_queue: asyncio.Queue,
        estop_queue: asyncio.Queue,
    ) -> None:
        self._cfg              = cfg
        self._state            = state
        self._nav_goal_queue   = nav_goal_queue
        self._unlock_queue     = unlock_queue
        self._estop_queue      = estop_queue

        # Internal client reference — set inside connect_loop context.
        self._client: Optional[aiomqtt.Client] = None
        self._connected        = False
        self._backoff          = cfg.reconnect_min_delay

        # Outbound publish queue — decouples publish() callers from the
        # underlying MQTT client lifecycle.
        # maxsize=256 prevents unbounded growth during long offline periods.
        self._pub_queue: asyncio.Queue = asyncio.Queue(maxsize=256)

    # ── Public API ────────────────────────────────────────────────────────────

    @property
    def is_connected(self) -> bool:
        return self._connected

    async def publish(
        self,
        topic: str,
        payload: dict | str | bytes,
        qos: int = 1,
        retain: bool = False,
    ) -> None:
        """
        Non-blocking publish. Enqueues the message for the send loop.
        If the outbound queue is full (offline for a long time), the oldest
        telemetry messages are dropped silently — QoS 0 topics only.
        Critical QoS 1/2 messages raise QueueFull so callers can retry.
        """
        if isinstance(payload, dict):
            payload = json.dumps(payload)

        try:
            self._pub_queue.put_nowait((topic, payload, qos, retain))
        except asyncio.QueueFull:
            if qos == 0:
                # Telemetry — safe to drop.
                logger.debug("Telemetry publish queue full, dropping QoS 0 message on %s", topic)
            else:
                # Critical command — propagate so caller can retry.
                raise

    # ── Main connection loop ──────────────────────────────────────────────────

    async def connect_loop(self) -> None:
        """
        Long-running coroutine. Establishes and maintains the MQTT connection.
        On disconnect, transitions the robot to OFFLINE_HOLD and retries with
        exponential backoff.

        This coroutine never returns — run it as an asyncio Task.
        """
        while True:
            try:
                await self._run_session()
            except aiomqtt.MqttError as exc:
                logger.warning(
                    "MQTT connection lost: %s. Reconnecting in %.1fs",
                    exc,
                    self._backoff,
                )
                await self._state.go_offline()
                await asyncio.sleep(self._backoff)
                self._backoff = min(self._backoff * 2, self._cfg.reconnect_max_delay)
            except Exception as exc:
                logger.error("Unexpected MQTT error: %s", exc, exc_info=True)
                await asyncio.sleep(self._backoff)

    # ── Session lifecycle ─────────────────────────────────────────────────────

    async def _run_session(self) -> None:
        """
        Runs a single MQTT session (connect → subscribe → process → disconnect).
        Exits when the connection drops, triggering reconnect in connect_loop.
        """
        logger.info(
            "Connecting to MQTT broker %s:%d as %s",
            self._cfg.host, self._cfg.port, self._cfg.client_id,
        )

        async with aiomqtt.Client(
            hostname=self._cfg.host,
            port=self._cfg.port,
            username=self._cfg.username,
            password=self._cfg.password,
            identifier=self._cfg.client_id,
            keepalive=self._cfg.keepalive,
            will=aiomqtt.Will(
                topic=Topics.HEARTBEAT,
                payload=json.dumps({
                    "source": "edge_daemon",
                    "status": "offline",
                    "client_id": self._cfg.client_id,
                }),
                qos=1,
                retain=False,
            ),
        ) as client:
            self._client = client
            self._connected = True
            self._backoff = self._cfg.reconnect_min_delay  # reset on success

            logger.info("MQTT connected. Subscribing to command topics.")
            await self._subscribe(client)

            # On reconnect, resume state (OFFLINE_HOLD → previous state).
            await self._state.go_online()

            # Run both the inbound message loop and the outbound publish loop
            # concurrently within this session.
            await asyncio.gather(
                self._inbound_loop(client),
                self._outbound_loop(client),
            )

    async def _subscribe(self, client: aiomqtt.Client) -> None:
        """Subscribe to all command topics on (re)connect."""
        subscriptions = [
            (Topics.NAVIGATE,    self._cfg.qos_commands),
            (Topics.UNLOCK,      self._cfg.qos_commands),
            (Topics.ESTOP,       self._cfg.qos_estop),
        ]
        for topic, qos in subscriptions:
            await client.subscribe(topic, qos=qos)
            logger.info("Subscribed to %s (QoS %d)", topic, qos)

    # ── Inbound message dispatch ──────────────────────────────────────────────

    async def _inbound_loop(self, client: aiomqtt.Client) -> None:
        """
        Processes inbound MQTT messages and routes them to the appropriate queue.
        This is the ONLY place where MQTT topics are dispatched — keep it thin.
        """
        async for message in client.messages:
            topic = str(message.topic)
            try:
                await self._dispatch(topic, message.payload)
            except Exception as exc:
                logger.error(
                    "Error dispatching message on topic %s: %s",
                    topic, exc, exc_info=True,
                )

    async def _dispatch(self, topic: str, raw: bytes) -> None:
        """Route a single inbound message to the correct handler queue."""
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            logger.warning("Non-JSON payload on topic %s: %r", topic, raw[:80])
            return

        if topic == Topics.NAVIGATE:
            await self._handle_navigate(data)
        elif topic == Topics.UNLOCK:
            # The unlock command is directed at the ESP32 via the broker,
            # but the edge daemon also observes it to transition to IDLE.
            await self._unlock_queue.put(data)
            logger.info(
                "Unlock command observed for order %s — queued for state update",
                data.get("order_id"),
            )
        elif topic == Topics.ESTOP:
            logger.critical("E-STOP received: %s", data)
            await self._estop_queue.put(data)
        else:
            logger.debug("Unhandled topic: %s", topic)

    async def _handle_navigate(self, data: dict) -> None:
        """
        Validates the navigate payload and enqueues a nav goal.

        Replay guard: reject messages where issued_at is more than 60 seconds
        in the past. This prevents stale MQTT messages (retained by the broker
        during an outage) from sending the robot to an outdated destination.
        """
        order_id  = data.get("order_id")
        issued_at = data.get("issued_at", 0)
        pose      = data.get("pose", {})
        map_frame = data.get("map_frame") or pose.get("frame") or "map"
        waypoint  = data.get("waypoint_name", "")

        if not order_id:
            logger.warning("Navigate payload missing order_id, ignoring")
            return

        age = time.time() - issued_at
        if age > 60:
            logger.warning(
                "Stale navigate command for order %s (age=%.0fs), ignoring",
                order_id, age,
            )
            return

        # Validate pose fields.
        x     = pose.get("x")
        y     = pose.get("y")
        theta = pose.get("theta", 0.0)

        if x is None or y is None:
            logger.error(
                "Navigate payload for order %s missing pose.x/y, ignoring",
                order_id,
            )
            return

        try:
            x = float(x)
            y = float(y)
            theta = float(theta)
        except (TypeError, ValueError):
            logger.error(
                "Navigate payload for order %s has non-numeric pose fields, ignoring",
                order_id,
            )
            return

        if not (math.isfinite(x) and math.isfinite(y) and math.isfinite(theta)):
            logger.error(
                "Navigate payload for order %s has non-finite pose fields, ignoring",
                order_id,
            )
            return

        goal = ActiveGoal(
            order_id=order_id,
            waypoint_name=waypoint,
            x=x,
            y=y,
            theta=theta,
            map_frame=str(map_frame),
            issued_at=time.monotonic(),
        )

        await self._nav_goal_queue.put(goal)
        logger.info(
            "Nav goal queued: order=%s waypoint=%s x=%.2f y=%.2f theta=%.2f",
            order_id, waypoint, x, y, theta,
        )

    # ── Outbound publish loop ─────────────────────────────────────────────────

    async def _outbound_loop(self, client: aiomqtt.Client) -> None:
        """
        Drains the outbound publish queue and sends messages to the broker.
        Runs concurrently with the inbound loop within the same MQTT session.
        """
        while True:
            topic, payload, qos, retain = await self._pub_queue.get()
            try:
                await client.publish(topic, payload=payload, qos=qos, retain=retain)
            except aiomqtt.MqttError as exc:
                logger.warning("Publish failed on %s: %s", topic, exc)
                # Re-enqueue critical messages, drop telemetry.
                if qos > 0:
                    try:
                        self._pub_queue.put_nowait((topic, payload, qos, retain))
                    except asyncio.QueueFull:
                        logger.error(
                            "Critical publish queue full, dropping %s message", topic
                        )
            finally:
                self._pub_queue.task_done()
