# edge_daemon/state_machine.py
#
# UnBot Delivery — Robot State Machine
# ──────────────────────────────────────
# Single source of truth for the robot's operational state on the edge.
# Protected by an asyncio.Lock — never a threading.Lock, because all
# mutations happen within the asyncio event loop. Using threading.Lock
# here would risk deadlocking the event loop if a coroutine awaits while
# holding it.
#
# STATE TRANSITIONS:
#
#   IDLE ──────────────────────────────► NAVIGATING
#     (nav goal received from MQTT)
#
#   NAVIGATING ────────────────────────► ARRIVED
#     (Nav2 action succeeded)
#
#   NAVIGATING ────────────────────────► FAULT
#     (Nav2 action failed or timed out)
#
#   NAVIGATING / ARRIVED ──────────────► OFFLINE_HOLD
#     (MQTT connection lost)
#
#   OFFLINE_HOLD ──────────────────────► NAVIGATING / ARRIVED
#     (MQTT reconnected — resume publishing, goal NOT re-issued)
#
#   ANY ────────────────────────────────► IDLE
#     (order completed — OTP validated, unlock command received from cloud)
#
#   ANY ────────────────────────────────► FAULT
#     (estop command received, nav timeout, or unrecoverable error)
#
#   FAULT ──────────────────────────────► IDLE
#     (manual reset via MQTT or operator dashboard)

from __future__ import annotations

import asyncio
import logging
import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional


logger = logging.getLogger(__name__)


class RobotState(Enum):
    IDLE         = auto()   # No active order. Robot is at rest or charging.
    NAVIGATING   = auto()   # Nav2 goal is active. Robot is moving.
    ARRIVED      = auto()   # Robot reached destination. Awaiting OTP unlock.
    OFFLINE_HOLD = auto()   # MQTT lost. Nav goal retained, no cloud publishing.
    FAULT        = auto()   # Navigation failed or estop triggered.


@dataclass
class ActiveGoal:
    """
    Persisted across state transitions so the daemon can resume publishing
    status after an OFFLINE_HOLD without re-issuing the goal to Nav2.
    """
    order_id:      str
    waypoint_name: str
    x:             float
    y:             float
    theta:         float
    map_frame:     str
    issued_at:     float    # Unix timestamp when the goal was injected into Nav2
    # Filled in as Nav2 reports progress.
    remaining_m:   float = 0.0
    progress_pct:  float = 0.0


class RobotStateMachine:
    """
    Thread-safe (asyncio) state machine for the robot.

    All public methods are async and acquire self._lock before mutating state.
    Callers from the MQTT bridge, nav bridge, and fault manager all go through
    this class — never mutate _state directly.
    """

    def __init__(self) -> None:
        self._state: RobotState = RobotState.IDLE
        self._prev_state: Optional[RobotState] = None
        self._active_goal: Optional[ActiveGoal] = None
        self._lock = asyncio.Lock()
        self._transition_time: float = time.monotonic()
        self._fault_reason: Optional[str] = None

        # Speed samples for ETA averaging (/odom callbacks update this).
        self._speed_samples: list[float] = []
        self._speed_window: int = 10

    # ── State accessors (lock-free reads — safe because GIL protects reads) ──

    @property
    def state(self) -> RobotState:
        return self._state

    @property
    def active_goal(self) -> Optional[ActiveGoal]:
        return self._active_goal

    @property
    def fault_reason(self) -> Optional[str]:
        return self._fault_reason

    @property
    def time_in_state(self) -> float:
        return time.monotonic() - self._transition_time

    @property
    def avg_speed_mps(self) -> float:
        if not self._speed_samples:
            return 0.0
        return sum(self._speed_samples) / len(self._speed_samples)

    # ── Transitions ───────────────────────────────────────────────────────────

    async def start_navigating(self, goal: ActiveGoal) -> bool:
        """
        Transition IDLE → NAVIGATING.
        Returns False if a goal is already active (caller should reject duplicate).
        """
        async with self._lock:
            if self._state not in (RobotState.IDLE, RobotState.FAULT):
                logger.warning(
                    "start_navigating rejected: already in state %s",
                    self._state.name,
                )
                return False
            self._active_goal = goal
            self._transition(RobotState.NAVIGATING)
            return True

    async def mark_arrived(self) -> None:
        """NAVIGATING → ARRIVED. Called by nav bridge on action success."""
        async with self._lock:
            if self._state == RobotState.NAVIGATING:
                self._transition(RobotState.ARRIVED)
            elif self._state == RobotState.OFFLINE_HOLD:
                # Goal completed while offline — update internally, will
                # publish ARRIVED status on reconnect.
                self._prev_state = RobotState.ARRIVED
            else:
                logger.warning(
                    "mark_arrived called from unexpected state %s",
                    self._state.name,
                )

    async def complete_delivery(self) -> None:
        """
        ANY → IDLE. Called when the OTP unlock command is received and
        processed. Clears the active goal.
        """
        async with self._lock:
            self._active_goal = None
            self._fault_reason = None
            self._speed_samples.clear()
            self._transition(RobotState.IDLE)

    async def go_offline(self) -> None:
        """
        NAVIGATING / ARRIVED → OFFLINE_HOLD.
        Preserves the active goal so the daemon does not re-issue it on reconnect.
        """
        async with self._lock:
            if self._state in (RobotState.NAVIGATING, RobotState.ARRIVED):
                self._prev_state = self._state
                self._transition(RobotState.OFFLINE_HOLD)

    async def go_online(self) -> None:
        """
        OFFLINE_HOLD → previous state (NAVIGATING or ARRIVED).
        Called on MQTT reconnection.
        """
        async with self._lock:
            if self._state == RobotState.OFFLINE_HOLD:
                resume = self._prev_state or RobotState.IDLE
                self._transition(resume)
                logger.info("Resumed state %s after reconnection", resume.name)

    async def trigger_fault(self, reason: str) -> None:
        """ANY → FAULT."""
        async with self._lock:
            self._fault_reason = reason
            self._transition(RobotState.FAULT)
            logger.error("FAULT: %s", reason)

    async def reset_fault(self) -> None:
        """FAULT → IDLE. Requires manual trigger from operator."""
        async with self._lock:
            if self._state == RobotState.FAULT:
                self._active_goal = None
                self._fault_reason = None
                self._transition(RobotState.IDLE)

    # ── Nav progress updates (called from nav bridge coroutine) ───────────────

    async def update_nav_progress(
        self,
        remaining_m: float,
        progress_pct: float,
        current_speed_mps: float,
    ) -> None:
        """
        Update progress fields on the active goal.
        Also maintains the rolling speed average for ETA computation.
        Called frequently (every Nav2 action feedback tick ~1Hz).
        """
        async with self._lock:
            if self._active_goal is not None:
                self._active_goal.remaining_m  = remaining_m
                self._active_goal.progress_pct = progress_pct

            # Rolling speed average — evict oldest if window full.
            self._speed_samples.append(current_speed_mps)
            if len(self._speed_samples) > self._speed_window:
                self._speed_samples.pop(0)

    def compute_eta_seconds(self, min_speed: float = 0.05) -> Optional[float]:
        """
        Compute ETA in seconds from remaining distance and average speed.
        Returns None if no active goal or speed is too low to estimate.
        """
        goal = self._active_goal
        if goal is None or goal.remaining_m <= 0:
            return None
        speed = self.avg_speed_mps
        if speed < min_speed:
            return None
        return goal.remaining_m / speed

    # ── Serialization (for persistence and MQTT telemetry) ───────────────────

    def to_telemetry_dict(self) -> dict:
        """
        Returns a dict suitable for inclusion in the robot/telemetry payload.
        Called from the telemetry collector — lock-free (atomic reads are safe).
        """
        goal = self._active_goal
        return {
            "nav_state":       self._state.name,
            "active_order_id": goal.order_id if goal else "",
            "remaining_m":     round(goal.remaining_m, 2) if goal else 0.0,
            "progress_pct":    round(goal.progress_pct, 1) if goal else 0.0,
            "avg_speed_mps":   round(self.avg_speed_mps, 3),
            "eta_seconds":     round(self.compute_eta_seconds() or 0, 1),
        }

    def goal_to_dict(self) -> Optional[dict]:
        """Returns the active goal as a dict for JSON persistence."""
        g = self._active_goal
        if g is None:
            return None
        return {
            "order_id":      g.order_id,
            "waypoint_name": g.waypoint_name,
            "x":             g.x,
            "y":             g.y,
            "theta":         g.theta,
            "map_frame":     g.map_frame,
            "issued_at":     g.issued_at,
        }

    # ── Internal ──────────────────────────────────────────────────────────────

    def _transition(self, new_state: RobotState) -> None:
        """
        Perform the state transition. MUST be called under self._lock.
        """
        if new_state == self._state:
            return
        logger.info(
            "State transition: %s -> %s (was in state for %.1fs)",
            self._state.name,
            new_state.name,
            time.monotonic() - self._transition_time,
        )
        self._state = new_state
        self._transition_time = time.monotonic()
