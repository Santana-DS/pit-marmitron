# ROS 2 Edge Validation

This checklist is for the Computacao team validating the app edge daemon against a ROS 2/Nav2 simulation or the real robot notebook.

The app team does not publish ROS commands directly. The contract is:

```text
Gateway/App -> MQTT robot/commands/navigate -> edge_daemon -> Nav2 NavigateToPose
Gateway/App -> MQTT robot/commands/estop    -> edge_daemon -> Nav2 cancel goal + FAULT
edge_daemon -> MQTT robot/telemetry         -> Gateway -> Operator app
```

## 1. Confirm ROS 2/Nav2 is alive

Run these on the notebook or simulation machine after sourcing the ROS 2 workspace:

```bash
ros2 action list
ros2 action info /navigate_to_pose
ros2 topic echo /amcl_pose --once
ros2 topic echo /odom --once
```

Expected:

- `/navigate_to_pose` exists.
- `/amcl_pose` is available when localization is running.
- `/odom` is available.

If the Nav2 action server name differs, set `NavConfig.action_server` or align the launch file before testing the app path.

## 2. Configure MQTT for the edge daemon

Create `Computacional/app/edge_daemon/.env` on the validation machine:

```dotenv
MQTT_HOST=<broker-host-or-ip>
MQTT_PORT=1883
MQTT_USER=<machine-user>
MQTT_PASSWORD=<machine-password>
MQTT_CLIENT_ID=unbot-edge-notebook-01
LOG_LEVEL=INFO
NAV_GOAL_TIMEOUT=120
TELEMETRY_HZ=2
```

Install the Python MQTT dependency if needed:

```bash
pip install aiomqtt
```

ROS 2 Python packages are expected to come from the ROS 2 installation, not from pip.

## 3. Start the edge daemon

From `Computacional/app`:

```bash
python -m edge_daemon
```

Expected log signs:

- `ROS 2 (rclpy) detected - FULL mode active.`
- `ROS 2 node initialized: unbot_edge_daemon`
- `Nav2 action server connected.`
- MQTT connected and subscribed to `robot/commands/navigate`, `robot/commands/unlock`, and `robot/commands/estop`.

If it prints stub mode, ROS 2 is not visible to that shell. Source the ROS setup file and try again.

## 4. Observe telemetry/status

In another terminal, from `Computacional/app`:

```bash
python -m edge_daemon.sim_command listen --seconds 60
```

Expected:

- `robot/status/heartbeat` arrives at QoS 1 cadence.
- `robot/telemetry` arrives with `nav_state`, `pose`, `velocity`, `battery`, and `active_order_id`.
- Pose frame is usually `map` from `/amcl_pose`; fallback may be `odom`.

## 5. Send a simulated navigation command

Use a safe coordinate in the current map:

```bash
python -m edge_daemon.sim_command navigate \
  --order-id SIM-001 \
  --x 1.0 \
  --y 2.0 \
  --theta 0.0 \
  --frame map \
  --waypoint SIM_POINT
```

Expected:

- The daemon logs `Nav goal queued` and `Nav2 accepted goal`.
- `ros2 action info /navigate_to_pose` shows activity while the goal is active.
- `robot/telemetry.nav_state` becomes `NAVIGATING`.
- `remaining_m`, `progress_pct`, and speed/ETA fields update as feedback arrives.
- On success, `robot/nav/status` publishes `ARRIVED` and telemetry eventually reports `ARRIVED`.

## 6. Validate emergency stop

While the simulated navigation goal is active:

```bash
python -m edge_daemon.sim_command estop --reason validation_button
```

Expected:

- The daemon logs `E-STOP received`.
- The daemon calls Nav2 goal cancellation.
- `robot/telemetry.nav_state` becomes `FAULT`.
- `fault_reason` in logs contains `estop: validation_button`.
- Nav2 no longer has the active goal.

This is the safety-critical acceptance test. Do not consider app integration validated until this works in the ROS simulation.

## 7. Validate through the Gateway/App

After the direct MQTT path works:

1. Start the Go gateway.
2. Start the mobile app.
3. Create/dispatch an order.
4. Confirm the gateway publishes `robot/commands/navigate`.
5. Confirm the edge daemon sends the goal to Nav2.
6. Open the operator screen and confirm live telemetry.
7. Press emergency stop from the operator screen and confirm the same Nav2 cancel behavior as step 6.

## Evidence to send back to the app team

Please send:

- The exact coordinate used for `SIM_POINT`.
- The daemon logs from one successful navigation.
- The daemon logs from one ESTOP during active navigation.
- One `robot/telemetry` sample while `NAVIGATING`.
- One `robot/telemetry` sample after `FAULT` or `ARRIVED`.
- Any mismatch in topic names, frame names, or Nav2 action server names.
