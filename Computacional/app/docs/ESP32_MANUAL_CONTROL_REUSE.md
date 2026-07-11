# ESP32 Manual Control Reuse

The robot folder contains a local ESP32 web control interface:

```text
Computacional/robot/Marmitron/src/webserver_module.cpp
```

It serves a phone-friendly HTML slider and sends:

```http
GET /set_speed?v=<float>
```

The firmware pushes that float into a FreeRTOS queue (`filaVelocidade`), and the motor task consumes it as the target velocity.

## What is reusable

- The UI concept: a simple operator speed slider.
- The firmware concept: non-blocking handoff from web request to motor task through a queue.
- The motor-side contract: a scalar velocity reference can be consumed by the PID loop.

## What is not safe to reuse directly

Do not embed the ESP32 local webserver directly into the cloud/operator app flow.

Reasons:

- The ESP endpoint is local LAN only.
- It has no authentication.
- It bypasses the gateway audit/control plane.
- It bypasses MQTT command semantics.
- It can conflict with autonomous navigation unless there is an explicit manual mode.
- It is not reachable when the robot is behind campus Wi-Fi/CGNAT, which is expected for the demo.

## Recommended cloud/app architecture

Use the ESP webserver as a lab/debug interface only. For the operator app, implement a cloud-mediated manual control path:

```text
Operator app -> Gateway REST -> MQTT robot/commands/manual_velocity
              -> edge_daemon/manual bridge -> robot motor controller
```

Minimum payload:

```json
{
  "source": "operator_app",
  "mode": "manual_velocity",
  "linear_mps": 0.25,
  "angular_radps": 0.0,
  "duration_ms": 300,
  "issued_at": 1714000000
}
```

Safety requirements:

- Manual mode must cancel or pause autonomous Nav2 goals first.
- Commands must expire quickly (`duration_ms`, dead-man switch).
- E-stop must override manual control.
- The edge daemon must drop stale commands.
- The operator UI must make manual mode visually distinct from autonomous mode.

## Current recommendation

Do not implement cloud manual control before ROS/Nav2 validation and camera streaming.

For demo value, video + map + e-stop are safer and more aligned with the project narrative. Manual motor control can be a later operator-only feature after the team defines safety interlocks.
