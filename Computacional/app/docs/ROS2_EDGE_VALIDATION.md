# ROS 2 Edge Validation

This checklist is for Computacao validating the app edge daemon against their current robot/simulation stack.

Important update: the robot material currently points to ROS 2 Foxy + ORB-SLAM3, not AMCL. The app edge daemon must therefore treat localized pose as a TF problem, not as an AMCL-only topic.

The app team does not publish ROS commands directly. The contract is:

```text
Gateway/App -> MQTT robot/commands/navigate -> edge_daemon -> Nav2 NavigateToPose
Gateway/App -> MQTT robot/commands/estop    -> edge_daemon -> Nav2 cancel goal + FAULT
edge_daemon -> MQTT robot/telemetry         -> Gateway -> Operator app
```

## 1. Confirm ROS 2 environment

The robot folder currently contains a Dockerfile based on:

```text
osrf/ros:foxy-desktop-focal
```

So either Foxy or Humble is acceptable, but the shell that starts `edge_daemon` must be sourced for the same ROS 2 environment used by the simulation/robot:

```bash
echo $ROS_DISTRO
ros2 topic list
ros2 node list
```

Expected:

- `rclpy` is importable from Python.
- `tf2_ros` is available.
- The edge daemon and the robot stack share the same `ROS_DOMAIN_ID`.

## 2. Confirm localization model

Do not require `/amcl_pose`.

Current Computacao material suggests ORB-SLAM3 publishes/derives the TF needed by Nav2, especially:

```text
map -> odom -> base_link
```

Validate TF:

```bash
ros2 run tf2_ros tf2_echo map base_link
ros2 run tf2_ros tf2_echo map odom
ros2 run tf2_ros tf2_echo odom base_link
```

Expected:

- `map -> base_link` resolves, either directly or through `map -> odom -> base_link`.
- The transform updates while the robot/simulation moves.

The edge daemon now derives localized telemetry from TF `map -> base_link` by default. If Computacao later exposes a pose topic, configure it explicitly with:

```dotenv
ROS_POSE_TOPIC=/some/localized_pose_topic
```

## 3. Confirm required topics

Recommended checks:

```bash
ros2 topic list
ros2 topic echo /odom --once
ros2 topic echo /imu --once
ros2 topic echo /camera/image_raw --once
```

Expected minimum for app telemetry:

- `/odom` for speed and odometry fallback.
- TF `map -> base_link` for localized pose.
- `/battery_state` if available; otherwise battery fields remain zero/empty until the BMS driver exists.

Expected for ORB-SLAM3:

- image topic consumed by ORB-SLAM3, often `image_raw`.
- IMU topic, currently `imu` in `serial_comms`.

If topic names differ, configure the edge daemon:

```dotenv
ROS_ODOM_TOPIC=/odom
ROS_BATTERY_TOPIC=/battery_state
ROS_TF_MAP_FRAME=map
ROS_TF_BASE_FRAME=base_link
```

## 4. Confirm Nav2 action server

The app navigation bridge still expects Nav2's `NavigateToPose` action.

```bash
ros2 action list
ros2 action info /navigate_to_pose
```

Expected:

- `/navigate_to_pose` exists if Nav2 is part of the current simulation.

If the action name differs:

```dotenv
NAV_ACTION_SERVER=<actual_action_name>
```

If Computacao is not using Nav2 yet, the edge daemon can still run in stub mode for app/gateway testing, but real dispatch-to-motion is not validated until an action server exists.

## 5. Configure MQTT for the edge daemon

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

ROS_ODOM_TOPIC=/odom
ROS_BATTERY_TOPIC=/battery_state
ROS_TF_MAP_FRAME=map
ROS_TF_BASE_FRAME=base_link
ROS_POSE_TOPIC=
```

Install the Python MQTT dependency if needed:

```bash
pip install aiomqtt
```

ROS 2 Python packages must come from the ROS 2 installation, not from pip.

## 6. Start the edge daemon

From `Computacional/app` after sourcing ROS:

```bash
python -m edge_daemon
```

Expected log signs:

- `ROS 2 (rclpy) detected - FULL mode active.`
- `ROS 2 node initialized: unbot_edge_daemon`
- `Localized pose topic disabled; deriving pose from TF map -> base_link`
- MQTT connected and subscribed to `robot/commands/navigate`, `robot/commands/unlock`, and `robot/commands/estop`.

If it prints stub mode, ROS 2 is not visible to that shell.

## 7. Observe telemetry/status

In another terminal, from `Computacional/app`:

```bash
python -m edge_daemon.sim_command listen --seconds 60
```

Expected:

- `robot/status/heartbeat` arrives at QoS 1 cadence.
- `robot/telemetry` arrives with `nav_state`, `pose`, `velocity`, `battery`, and `active_order_id`.
- `pose.frame` should be `map` when TF `map -> base_link` resolves.
- If TF is not available, pose falls back to `/odom`.

## 8. Send a simulated navigation command

Before real route execution, validate geographic conversion with the route
datum supplied by Computacao:

```bash
ros2 service list | grep fromLL
```

Expected: `/fromLL` is offered by `navsat_transform_node`. The route executor
uses this service to convert each approved GPS node to the Nav2 `map` frame.
Computacao must provide the ordered, rehearsed GPS nodes; the app team stores
and dispatches them, but never generates a route from a destination alone.

Use a safe coordinate in the current Nav2 map:

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

## 9. Validate emergency stop

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

This is the safety-critical acceptance test.

## Evidence to send back to the app team

Please send:

- `echo $ROS_DISTRO`.
- Output of `ros2 topic list`.
- Output of `ros2 action list`.
- Whether `tf2_echo map base_link` resolves.
- One `robot/telemetry` sample while idle.
- One `robot/telemetry` sample while navigating.
- One ESTOP test log.
- Any mismatch in topic names, frame names, or Nav2 action server names.
