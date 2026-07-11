# Robot Stack Audit Notes

This note summarizes integration-relevant findings from `Computacional/robot`.

## ROS stack mismatch found

The robot material currently points to:

- ROS 2 Foxy (`osrf/ros:foxy-desktop-focal`)
- ORB-SLAM3 ROS2 wrapper
- `v4l2_camera`
- `serial_comms` publishing IMU on topic `imu`
- ORB-SLAM3 consuming image topic `image_raw`
- ORB-SLAM3 attempting to publish/derive TF `map -> odom`

This means app docs/edge daemon must not require AMCL. Localized telemetry should be accepted from TF:

```text
map -> odom -> base_link
```

The edge daemon was updated to derive pose from TF `map -> base_link` by default, with optional `ROS_POSE_TOPIC` support if a pose topic is later provided.

## ORB-SLAM3 wrapper risks to send back to Computacao

File:

```text
Computacional/robot/Modified zang9 nodes/mono-inertial/mono-inertial-node.cpp
```

Observed risks:

- `GrabImage()` calls `bufImgMutex_.lock()` twice and never unlocks. This can deadlock image processing after the first image callback.
- `SyncWithImu_Track()` reads `imgBuf_.front()` before checking whether the image queue is empty. This can crash if the tracking thread starts before an image arrives.
- TF translation assignment appears to write `translation.x` three times instead of x/y/z:

```cpp
transf_msg.transform.translation.x = Tmo.translation().x();
transf_msg.transform.translation.x = Tmo.translation().y();
transf_msg.transform.translation.x = Tmo.translation().z();
```

Expected:

```cpp
transf_msg.transform.translation.x = Tmo.translation().x();
transf_msg.transform.translation.y = Tmo.translation().y();
transf_msg.transform.translation.z = Tmo.translation().z();
```

- The node subscribes to relative topics `image_raw` and `imu`. Depending on namespace/remaps, this may not match `/camera/image_raw` and `/imu`.

These are not app-owned changes, but they affect whether `tf2_echo map base_link` can ever resolve reliably.

## ESP32 local control found

File:

```text
Computacional/robot/Marmitron/src/webserver_module.cpp
```

It exposes:

```http
GET /set_speed?v=<float>
```

This is useful as a local lab/debug controller, but should not be directly embedded in the operator app or exposed through the cloud without safety interlocks. See `ESP32_MANUAL_CONTROL_REUSE.md`.
