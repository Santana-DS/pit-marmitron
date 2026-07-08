# Robot Camera Streaming

The operator screen must show the robot camera in real time. The camera is a Logitech C920 connected to the onboard x86 notebook.

## Recommendation

Use WebRTC for the production/demo path when the operator app is not guaranteed to be on the same LAN as the robot.

Why:

- Lower latency than HLS.
- Better behavior on unstable Wi-Fi/LTE.
- Designed for NAT traversal and relay.
- Avoids pushing raw camera frames through MQTT.

MQTT remains for commands and telemetry only. Video must use a video transport.

## Practical rollout

### Phase 1 - proof of life

Goal: show the C920 image in the operator screen during lab validation.

Acceptable options:

- MJPEG HTTP stream, if the app and notebook are on the same LAN.
- RTSP restreamed to a URL reachable by the operator device.
- A WebRTC test page/endpoint if the computation team already has a stack ready.

Gateway contract:

```http
GET /api/robot/camera
```

Response when configured:

```json
{
  "available": true,
  "stream_url": "http://notebook-or-relay:8081/stream.mjpg",
  "stream_kind": "mjpeg",
  "label": "C920 front camera",
  "latency_target_ms": 500
}
```

Response when not configured:

```json
{
  "available": false,
  "stream_url": "",
  "stream_kind": "unset",
  "label": "Robot camera",
  "latency_target_ms": 500
}
```

Gateway environment variables:

```dotenv
ROBOT_CAMERA_STREAM_URL=
ROBOT_CAMERA_STREAM_KIND=unset
ROBOT_CAMERA_LABEL=Robot camera
ROBOT_CAMERA_LATENCY_TARGET_MS=500
```

The gateway only exposes metadata. It does not proxy the video in Phase 1.

### Phase 2 - real remote demo

Goal: operator can view video when the robot notebook is behind campus Wi-Fi/NAT.

Recommended topology:

```text
C920 -> notebook video process -> WebRTC publisher -> relay/SFU -> Flutter operator viewer
```

The relay/SFU can run on EC2 or another public host. The notebook should initiate outbound connections when possible; do not assume the phone can open an inbound connection to the notebook.

### Phase 3 - ROS integration

If navigation also consumes the C920 through ROS 2:

```text
C920 -> v4l2_camera/usb_cam -> /camera/image_raw or /camera/image_raw/compressed
```

The app-facing video bridge can either read from the camera device directly or subscribe to a compressed ROS image topic. Do not publish image frames over MQTT.

## Acceptance criteria

- Operator screen shows a camera panel.
- If no stream is configured, it clearly says camera stream is not configured.
- If a stream URL is configured, the app attempts to render it and exposes the URL/kind for debugging.
- E-stop remains visible and reachable without scrolling past the video on common phone sizes.
- Video failure must not block telemetry or e-stop.

## Open decisions for Computacao

- Which Linux device path does the C920 use (`/dev/video0`, etc.)?
- What resolution and FPS are stable on the notebook?
- Does navigation need raw ROS image topics, compressed topics, or both?
- Is the operator device on the same LAN during demo?
- If not same LAN, which relay/WebRTC service will be used?
