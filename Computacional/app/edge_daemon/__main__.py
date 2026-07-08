# edge_daemon/__main__.py
#
# UnBot Delivery — Edge Daemon Entrypoint
# ─────────────────────────────────────────────────────────────────────────────
# Invocation:
#   python -m edge_daemon                          (reads edge_daemon/.env)
#   MQTT_HOST=1.2.3.4 MQTT_USER=u MQTT_PASSWORD=p python -m edge_daemon
#
# STUB MODE (no ROS 2 / no robot hardware):
#   Runs automatically when `rclpy` is not installed — detected at import time
#   inside nav_bridge.py and telemetry.py. In stub mode:
#     - NavBridge simulates a 10-second delivery with decreasing remaining_m.
#     - TelemetryCollector publishes zeroed pose/battery (no /odom subscription).
#     - All MQTT topics, state machine transitions, and heartbeat cadence are
#       IDENTICAL to production — only the ROS 2 I/O is replaced.
#   This means you can run `python -m edge_daemon` on Windows/Mac right now
#   and observe real telemetry flowing into the Go backend.
#
# TASK TOPOLOGY (asyncio):
#
#   ┌─────────────────────────────────────────────────┐
#   │                  asyncio event loop              │
#   │                                                  │
#   │  MQTTBridge.connect_loop() ◄──────────────────┐ │
#   │       │ nav_goal_queue                         │ │
#   │       ▼                                        │ │
#   │  NavBridge.run()                               │ │
#   │       │ state machine mutations                │ │
#   │       ▼                                        │ │
#   │  TelemetryCollector.run() ─── publish() ───────┘ │
#   │                                                  │
#   │  _unlock_observer()  (completes delivery)        │
#   │  _fault_monitor()    (watchdog)                  │
#   │  _shutdown_handler() (SIGINT / SIGTERM)          │
#   └─────────────────────────────────────────────────┘
#
# SHUTDOWN:
#   SIGINT or SIGTERM cancels all tasks cleanly.
#   Each task catches asyncio.CancelledError and logs its exit.
#
# QUEUES:
#   nav_goal_queue  — MQTTBridge → NavBridge   (ActiveGoal objects)
#   unlock_queue    — MQTTBridge → _unlock_observer (dict payloads)
#   Both are unbounded for goals (low volume); publish queue inside
#   MQTTBridge is capped at 256 (see mqtt_bridge.py).
# ─────────────────────────────────────────────────────────────────────────────

from __future__ import annotations

import asyncio
import logging
import os
import signal
import sys
from typing import Optional

# ── Minimal dotenv loader (mirrors config.py's _load_dotenv) ─────────────────
# Loaded BEFORE config.load() so environment variables are populated when
# config reads them. Using the same file-adjacent logic as config.py.
def _bootstrap_dotenv() -> None:
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if not os.path.exists(env_path):
        return
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key   = key.strip()
            value = value.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = value

_bootstrap_dotenv()

# ── Logging — configured before any module import so all loggers pick it up ──
def _configure_logging(level_str: str) -> None:
    level = getattr(logging, level_str.upper(), logging.INFO)
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)-8s] %(name)s: %(message)s",
        datefmt="%Y-%m-%dT%H:%M:%S",
        stream=sys.stdout,
    )
    # Suppress noisy aiomqtt internals at INFO level.
    logging.getLogger("aiomqtt").setLevel(logging.WARNING)

# Peek at LOG_LEVEL before full config load so logging works during import.
_configure_logging(os.environ.get("LOG_LEVEL", "INFO"))

logger = logging.getLogger("edge_daemon.main")

# ── Dependency check — warn early about missing optional packages ─────────────
def _check_dependencies() -> None:
    missing = []
    try:
        import aiomqtt  # noqa: F401
    except ImportError:
        missing.append("aiomqtt")

    if missing:
        logger.critical(
            "Required packages missing: %s\n"
            "Install with:  pip install %s",
            ", ".join(missing),
            " ".join(missing),
        )
        sys.exit(1)

    try:
        import rclpy  # noqa: F401
        logger.info("ROS 2 (rclpy) detected — FULL mode active.")
    except ImportError:
        logger.warning(
            "rclpy not found — running in STUB MODE.\n"
            "    NavBridge will simulate a 10-second delivery.\n"
            "    TelemetryCollector will publish zeroed pose/battery.\n"
            "    All MQTT topics and state machine logic are identical to production."
        )

_check_dependencies()

# ── Internal imports (after dependency check so errors are readable) ──────────
from .config import load_config                          # noqa: E402
from .state_machine import RobotStateMachine             # noqa: E402
from .mqtt_bridge import MQTTBridge                      # noqa: E402
from .nav_bridge import NavBridge                        # noqa: E402
from .telemetry import TelemetryCollector                # noqa: E402


def _init_ros2_once() -> None:
    """Initialize rclpy once before ROS-using tasks start their threads."""
    try:
        import rclpy
    except ImportError:
        return

    if not rclpy.ok():
        rclpy.init()
        logger.info("ROS 2 rclpy initialized.")


# ─────────────────────────────────────────────────────────────────────────────
# Background task: observe unlock commands → complete delivery in state machine
# ─────────────────────────────────────────────────────────────────────────────

async def _unlock_observer(
    unlock_queue: asyncio.Queue,
    state: RobotStateMachine,
) -> None:
    """
    Drains unlock_queue and transitions the state machine to IDLE.

    The unlock command is published by the Go gateway to robot/commands/unlock
    after successful OTP validation. The ESP32 fires the solenoid; this observer
    updates the edge daemon's state so it stops publishing nav status for the
    completed order and clears the active_goal reference.

    This task runs forever and exits only on CancelledError (shutdown).
    """
    logger.info("Unlock observer started.")
    try:
        while True:
            payload: dict = await unlock_queue.get()
            order_id = payload.get("order_id", "<unknown>")
            logger.info(
                "Unlock command observed for order %s — marking delivery complete.",
                order_id,
            )
            await state.complete_delivery()
            unlock_queue.task_done()
            logger.info("State machine transitioned to IDLE after order %s.", order_id)
    except asyncio.CancelledError:
        logger.info("Unlock observer cancelled.")
        raise


# ─────────────────────────────────────────────────────────────────────────────
# Background task: fault watchdog
# ─────────────────────────────────────────────────────────────────────────────

async def _estop_observer(
    estop_queue: asyncio.Queue,
    state: RobotStateMachine,
    nav_bridge: NavBridge,
) -> None:
    """Cancel the active Nav2 goal before marking the robot as FAULT."""
    logger.info("E-stop observer started.")
    try:
        while True:
            payload: dict = await estop_queue.get()
            reason = payload.get("reason", "unspecified")
            logger.critical("E-STOP received: %s", payload)
            await nav_bridge.cancel_current_goal()
            await state.trigger_fault(f"estop: {reason}")
            estop_queue.task_done()
            logger.critical("E-STOP applied: Nav2 goal cancelled and state set to FAULT")
    except asyncio.CancelledError:
        logger.info("E-stop observer cancelled.")
        raise


async def _fault_monitor(
    state: RobotStateMachine,
    nav_bridge: NavBridge,
    offline_timeout: float,
    cancel_on_timeout: bool,
) -> None:
    """
    Monitors for prolonged OFFLINE_HOLD and triggers a safe-stop fault.

    If the robot loses MQTT connectivity for longer than `offline_timeout`
    seconds while navigating, we transition to FAULT so the robot's own
    Nav2 recovery behaviours engage (or a human intervenes).

    On a development machine in stub mode this will never fire unless you
    intentionally kill the broker connection for > offline_timeout seconds.
    """
    from .state_machine import RobotState  # local import to avoid circular dep

    logger.info(
        "Fault monitor started (offline_timeout=%.0fs, cancel_on_timeout=%s).",
        offline_timeout,
        cancel_on_timeout,
    )
    try:
        while True:
            await asyncio.sleep(10)  # check every 10 s
            if state.state == RobotState.OFFLINE_HOLD:
                time_offline = state.time_in_state
                if time_offline >= offline_timeout:
                    logger.error(
                        "Robot has been OFFLINE_HOLD for %.0fs (threshold=%.0fs). "
                        "Triggering fault.",
                        time_offline,
                        offline_timeout,
                    )
                    if cancel_on_timeout:
                        await nav_bridge.cancel_current_goal()
                        await state.trigger_fault(
                            f"offline_hold_timeout after {time_offline:.0f}s"
                        )
    except asyncio.CancelledError:
        logger.info("Fault monitor cancelled.")
        raise


# ─────────────────────────────────────────────────────────────────────────────
# Graceful shutdown
# ─────────────────────────────────────────────────────────────────────────────

def _install_signal_handlers(
    loop: asyncio.AbstractEventLoop,
    shutdown_event: asyncio.Event,
) -> None:
    """
    Register SIGINT / SIGTERM handlers that set the shutdown_event.

    On Windows, asyncio loop.add_signal_handler is NOT supported.
    We fall back to the default KeyboardInterrupt mechanism there
    (Ctrl-C raises KeyboardInterrupt which is caught in main()).
    """
    
    if sys.platform == "win32":
        logger.debug(
            "Windows detected — signal handlers not installed. "
            "Use Ctrl-C to stop."
        )
        return

    def _handle(signame: str) -> None:
        logger.info("Signal %s received — initiating graceful shutdown.", signame)
        shutdown_event.set()

    loop.add_signal_handler(signal.SIGINT,  lambda: _handle("SIGINT"))
    loop.add_signal_handler(signal.SIGTERM, lambda: _handle("SIGTERM"))


# ─────────────────────────────────────────────────────────────────────────────
# Main coroutine
# ─────────────────────────────────────────────────────────────────────────────

async def _amain() -> None:
    """
    Builds every component, wires the shared queues, and runs all tasks
    concurrently until a shutdown signal arrives.
    """
    # ── Config ────────────────────────────────────────────────────────────────
    try:
        cfg = load_config()
    except RuntimeError as exc:
        logger.critical("Configuration error: %s", exc)
        sys.exit(1)

    # Re-apply log level from config (may differ from bootstrap peek above).
    _configure_logging(cfg.log_level)

    logger.info(
        "Edge daemon starting — broker=%s:%d client_id=%s",
        cfg.mqtt.host,
        cfg.mqtt.port,
        cfg.mqtt.client_id,
    )

    _init_ros2_once()

    # ── Shared state ──────────────────────────────────────────────────────────
    state = RobotStateMachine()

    # ── Inter-task queues ─────────────────────────────────────────────────────
    # maxsize=0 → unbounded. Goals and unlock commands are rare; no cap needed.
    nav_goal_queue: asyncio.Queue = asyncio.Queue()
    unlock_queue:   asyncio.Queue = asyncio.Queue()
    estop_queue:    asyncio.Queue = asyncio.Queue()

    # ── Component construction ────────────────────────────────────────────────
    mqtt_bridge = MQTTBridge(
        cfg=cfg.mqtt,
        state=state,
        nav_goal_queue=nav_goal_queue,
        unlock_queue=unlock_queue,
        estop_queue=estop_queue,
    )

    nav_bridge = NavBridge(
        cfg=cfg.nav,
        state=state,
        nav_goal_queue=nav_goal_queue,
        mqtt_bridge=mqtt_bridge,
    )

    telemetry_collector = TelemetryCollector(
        tel_cfg=cfg.telemetry,
        nav_cfg=cfg.nav,
        state=state,
        mqtt_bridge=mqtt_bridge,
    )

    # ── Shutdown event ────────────────────────────────────────────────────────
    loop = asyncio.get_running_loop()
    shutdown_event = asyncio.Event()
    _install_signal_handlers(loop, shutdown_event)

    # ── Task table ────────────────────────────────────────────────────────────
    # Each entry: (name, coroutine)
    # All tasks are cancelled together on shutdown; order doesn't matter because
    # they communicate through queues, not direct calls.
    task_defs = [
        ("mqtt_bridge",        mqtt_bridge.connect_loop()),
        ("nav_bridge",         nav_bridge.run()),
        ("telemetry",          telemetry_collector.run()),
        ("unlock_observer",    _unlock_observer(unlock_queue, state)),
        ("estop_observer",     _estop_observer(estop_queue, state, nav_bridge)),
        ("fault_monitor",      _fault_monitor(
                                   state,
                                   nav_bridge,
                                   cfg.fault.offline_safe_stop_timeout,
                                   cfg.fault.cancel_goal_on_offline_timeout,
                               )),
    ]

    tasks: list[asyncio.Task] = [
        asyncio.create_task(coro, name=name)
        for name, coro in task_defs
    ]

    logger.info(
        "All tasks started (%d). MQTT → %s:%d | telemetry %.1f Hz | heartbeat every %.0fs.",
        len(tasks),
        cfg.mqtt.host,
        cfg.mqtt.port,
        cfg.telemetry.publish_hz,
        cfg.telemetry.heartbeat_interval,
    )

    # ── Run until shutdown or a task crashes ──────────────────────────────────
    # asyncio.wait() with FIRST_EXCEPTION lets us detect a task crash and
    # shut everything else down rather than running in a broken half-state.
    shutdown_task = asyncio.create_task(shutdown_event.wait(), name="shutdown_sentinel")
    all_tasks = tasks + [shutdown_task]

    done, pending = await asyncio.wait(
        all_tasks,
        return_when=asyncio.FIRST_COMPLETED,
    )

    # Identify what triggered the wakeup.
    for finished in done:
        if finished is shutdown_task:
            logger.info("Shutdown event set — stopping all tasks.")
        else:
            exc = finished.exception()
            if exc is not None:
                logger.critical(
                    "Task '%s' crashed with %s: %s — initiating emergency shutdown.",
                    finished.get_name(),
                    type(exc).__name__,
                    exc,
                    exc_info=exc,
                )
            else:
                # A task returned normally (should not happen for long-running tasks).
                logger.warning(
                    "Task '%s' exited unexpectedly without an exception.",
                    finished.get_name(),
                )

    # ── Cancel every remaining task ───────────────────────────────────────────
    for task in pending:
        task.cancel()

    # Wait for all cancellations to propagate.
    results = await asyncio.gather(*pending, return_exceptions=True)
    for task, result in zip(pending, results):
        if isinstance(result, Exception) and not isinstance(result, asyncio.CancelledError):
            logger.error(
                "Task '%s' raised during shutdown: %s",
                task.get_name(),
                result,
            )

    nav_bridge.close()
    try:
        import rclpy

        if rclpy.ok():
            rclpy.shutdown()
            logger.info("ROS 2 rclpy shut down.")
    except ImportError:
        pass

    logger.info("Edge daemon stopped cleanly.")


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    """
    Synchronous wrapper so the module can be invoked as:
      python -m edge_daemon
    or called from a systemd ExecStart.
    """
    try:
        if sys.platform == "win32":
            # O aiomqtt exige o SelectorEventLoop no Windows para suportar add_reader()
            asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

        asyncio.run(_amain())

    except KeyboardInterrupt:
        # Ctrl-C on Windows (no signal handler installed).
        logger.info("KeyboardInterrupt — edge daemon stopped.")
    except Exception as exc:
        logger.critical("Unhandled exception in main: %s", exc, exc_info=True)
        sys.exit(1)


if __name__ == "__main__":
    main()
