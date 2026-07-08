# Robot Camera Streaming

The operator screen must show the robot camera in real time. The camera is a Logitech C920 connected to the onboard x86 notebook.

## Decisions and assumptions

These assumptions are now the working contract until Computacao proves otherwise:

- The demo operator device will not be on the same LAN as the robot notebook.
- The camera must also be consumed by ROS 2 for navigation/perception.
- The C920 is connected to the notebook as a Linux V4L2 camera, assumed `/dev/video0`.
- ROS 2 publishes the camera through `v4l2_camera` or `usb_cam`.
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

Suggested app-video direction:

```text
ROS compressed image topic or /dev/video0 -> WebRTC publisher -> public relay
```

The WebRTC publisher may read directly from `/dev/video0` or from a ROS compressed image topic. Reading from the ROS compressed topic keeps navigation and operator video aligned, but may add CPU overhead. Direct `/dev/video0` capture may be simpler but must not starve the ROS camera node.

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
