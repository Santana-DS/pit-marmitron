# Robot Camera Streaming

The operator screen must show the robot camera in real time. The camera is a Logitech C920 connected to the onboard x86 notebook.

## Decisions and assumptions

These assumptions are now the working contract until Computacao proves otherwise:

- The demo operator device will not be on the same LAN as the robot notebook.
- The camera must also be consumed by ROS 2 for navigation/perception.
- The C920 is connected to the notebook as a Linux V4L2 camera, assumed `/dev/video0`.
- ROS 2 publishes the camera through `v4l2_camera` or `usb_cam`.
- The physical V4L2 device belongs exclusively to the ROS 2 camera node.
- The primary ROS topics are assumed to be:
  - `/camera/image_raw`
  - `/camera/image_raw/compressed`
- Target camera mode for the demo: 1280x720 at 30 FPS, reduced to 640x480 at 15 FPS if CPU/network is unstable.
- MQTT is never used for video frames.
- WebRTC with a public relay/signaling service is the target for the remote demo.
- MJPEG/HTTP is only a local-lab fallback, not the final remote-demo plan.

## Recommended architecture

```text
                         ROS 2 path
                         ---------
C920 -> notebook camera driver -> /camera/image_raw or /camera/image_raw/compressed
                         |
                         | app video path
                         v
                 WebRTC publisher on notebook
                         |
                         v
                public relay/signaling service
                         |
                         v
                Flutter operator camera panel
```

The notebook should initiate outbound connections to the public relay when possible. Do not assume a phone or evaluator device can open an inbound connection to the notebook on campus Wi-Fi.

## Critical Linux/V4L2 rule

Do not run two independent processes against `/dev/video0`.

Most Linux V4L2 camera devices, including typical C920 setups, do not reliably allow multiple simultaneous readers. If `v4l2_camera` or `usb_cam` owns `/dev/video0` and a second MJPEG/WebRTC process also tries to open `/dev/video0`, the second process may fail with:

```text
Device or resource busy
```

Rule:

```text
/dev/video0 -> ROS 2 camera node -> /camera/image_raw/compressed -> app video bridge
```

The app-facing video bridge must consume the ROS 2 compressed image topic or a deliberate ROS-derived restream. It must not compete with the ROS 2 camera node for the physical camera device.

## Gateway contract

The gateway exposes camera metadata to the operator app. It does not proxy video frames.

```http
GET /api/robot/camera
```

Recommended WebRTC response:

```json
{
  "available": true,
  "stream_url": "https://relay.example/unbot/camera/front",
  "stream_kind": "webrtc",
  "signaling_url": "wss://relay.example/unbot/signaling",
  "ice_servers": ["stun:stun.l.google.com:19302"],
  "label": "C920 front camera",
  "latency_target_ms": 500,
  "ros_image_topic": "/camera/image_raw",
  "ros_compressed_topic": "/camera/image_raw/compressed"
}
```

Response when not configured:

```json
{
  "available": false,
  "stream_url": "",
  "stream_kind": "unset",
  "signaling_url": "",
  "ice_servers": [],
  "label": "C920 front camera",
  "latency_target_ms": 500,
  "ros_image_topic": "/camera/image_raw",
  "ros_compressed_topic": "/camera/image_raw/compressed"
}
```

Gateway environment variables:

```dotenv
ROBOT_CAMERA_STREAM_URL=
ROBOT_CAMERA_STREAM_KIND=unset
ROBOT_CAMERA_SIGNALING_URL=
ROBOT_CAMERA_ICE_SERVERS=stun:stun.l.google.com:19302
ROBOT_CAMERA_LABEL=C920 front camera
ROBOT_CAMERA_LATENCY_TARGET_MS=500
ROBOT_CAMERA_ROS_IMAGE_TOPIC=/camera/image_raw
ROBOT_CAMERA_ROS_COMPRESSED_TOPIC=/camera/image_raw/compressed
```

For a local fallback, set:

```dotenv
ROBOT_CAMERA_STREAM_URL=http://notebook-lan-ip:8081/stream.mjpg
ROBOT_CAMERA_STREAM_KIND=mjpeg
```

For the remote demo, set:

```dotenv
ROBOT_CAMERA_STREAM_URL=https://relay.example/unbot/camera/front
ROBOT_CAMERA_STREAM_KIND=webrtc
ROBOT_CAMERA_SIGNALING_URL=wss://relay.example/unbot/signaling
```

## Computacao setup expectation

Computacao should provide two outputs from the notebook:

1. ROS 2 camera topics for navigation/perception.
2. A video publisher path for the operator app.

Suggested ROS 2 validation:

```bash
ros2 topic list | grep camera
ros2 topic echo /camera/image_raw --once
ros2 topic hz /camera/image_raw
ros2 topic hz /camera/image_raw/compressed
```

Suggested camera-driver direction:

```text
C920 -> v4l2_camera or usb_cam -> ROS image topics
```

We provide a starter launch file for the C920:

```text
Computacional/app/edge_daemon/ros2_camera/launch/c920_camera.launch.py
```

It assumes `v4l2_camera`, `/dev/video0`, 1280x720 at 30 FPS, and the ROS namespace `/camera`. Computacao should copy/adapt it into their ROS workspace if their simulation/robot launch stack needs a different device path, frame id, or resolution.

Suggested app-video direction:

```text
ROS compressed image topic -> WebRTC publisher -> public relay
```

The WebRTC publisher should read from `/camera/image_raw/compressed` or from a local restream derived from that ROS topic. It must not read `/dev/video0` directly while the ROS 2 camera node is running.

## Suggested media server shortcut

Do not build WebRTC signaling from scratch for the demo.

Recommended candidates:

- MediaMTX (formerly rtsp-simple-server)
- go2rtc

Both can run on the notebook or on a small public relay host and provide WebRTC-oriented serving/signaling without us writing the signaling layer ourselves.

Suggested shape:

```text
/camera/image_raw/compressed -> small ROS bridge/restream -> MediaMTX/go2rtc -> WebRTC -> Flutter operator
```

If MediaMTX/go2rtc runs on the notebook, remote access still needs a reachable public endpoint, VPN, tunnel, or relay. Because the demo device will not be on the same LAN, prefer a public relay or an outbound connection from the notebook to the relay.

## Operator app behavior

- The operator panel asks the gateway for `GET /api/robot/camera`.
- If `stream_kind` is `mjpeg`, the current app attempts inline `Image.network` rendering.
- If `stream_kind` is `webrtc`, the current app shows that WebRTC is configured but still needs a dedicated WebRTC viewer package.
- Video errors must not block telemetry, navigation status, or e-stop.
- E-stop must remain reachable even if video fails.

## Next implementation step

Add a Flutter WebRTC viewer when the relay/signaling endpoint is known. Candidate app-side dependency: `flutter_webrtc`.

Do not add this dependency until the relay choice is known, because the signaling handshake shape differs between WebRTC services.

## Acceptance criteria

- ROS 2 can consume the C920 through `/camera/image_raw` or `/camera/image_raw/compressed`.
- Gateway returns camera metadata through `GET /api/robot/camera`.
- Operator screen shows camera availability and stream kind.
- Remote-demo plan uses WebRTC/relay, not direct notebook LAN access.
- MJPEG is treated as local fallback only.
