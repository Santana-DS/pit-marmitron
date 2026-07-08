# C920 ROS 2 Camera Starter

This folder is a starter kit for the Computacao team. It is intentionally small: one launch file and one parameter file to make the Logitech C920 publish a stable ROS 2 image topic.

It does not replace the team's navigation launch files. Include or copy it into the robot workspace only after confirming the actual device path and desired resolution.

## Safety rule

Only the ROS 2 camera node should open the physical camera device.

```text
/dev/video0 -> v4l2_camera -> /camera/image_raw -> /camera/image_raw/compressed -> app video bridge
```

Do not run a separate MJPEG/WebRTC process directly against `/dev/video0` while `v4l2_camera` is running. Many V4L2 devices reject the second reader with `Device or resource busy`.

## Install expected packages

On ROS 2 Humble:

```bash
sudo apt install ros-humble-v4l2-camera ros-humble-image-transport-plugins
```

## Launch

From `Computacional/app` after sourcing ROS 2:

```bash
ros2 launch edge_daemon/ros2_camera/launch/c920_camera.launch.py
```

Override defaults when needed:

```bash
ros2 launch edge_daemon/ros2_camera/launch/c920_camera.launch.py \
  video_device:=/dev/video2 \
  image_width:=640 \
  image_height:=480 \
  fps:=15
```

## Expected topics

The launch file targets:

```text
/camera/image_raw
/camera/image_raw/compressed
```

Depending on installed `image_transport` plugins, the compressed topic may appear only when a compressed subscriber exists. If it does not appear automatically, Computacao should add an explicit `image_transport republish` node or a small ROS bridge from raw to compressed.

## Validate

```bash
ros2 topic list | grep camera
ros2 topic hz /camera/image_raw
ros2 topic echo /camera/image_raw --once
```

If compressed is available:

```bash
ros2 topic hz /camera/image_raw/compressed
```

## App/video bridge

The operator app should not consume ROS directly. The app-facing media process should consume `/camera/image_raw/compressed` or a ROS-derived restream and publish WebRTC through a relay/signaling service such as MediaMTX or go2rtc.
