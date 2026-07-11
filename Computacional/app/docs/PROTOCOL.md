# UnBot Delivery — Protocol Contracts

## REST API (Go gateway · `HTTP :8080`)

Base URL (dev tunnel): `https://rvdj88q6-8000.brs.devtunnels.ms`  
Override at Flutter build time: `--dart-define=API_BASE_URL=http://10.0.2.2:8080`

All request/response bodies are `application/json`. All error responses share the shape `{"error": "<message>"}`.

---

### `GET /health`

Health probe. No auth required.

**Response 200**
```json
{ "status": "ok", "version": "2.2.0" }
```

---

### `POST /api/orders/{id}/dispatch`

Orchestrates a delivery. Issues a cryptographically random 4-digit OTP and publishes a navigate command to ROS 2 via MQTT.

**Path parameter**: `id` — caller-generated order ID (e.g. `order_1714000000123`).

**Request body**
```json
{
  "destination": { "x": 12.0, "y": -3.5 },
  "restaurant_name": "Marmitas da Vó"
}
```

Coordinate validation: `x` and `y` must be finite floats (NaN and ±Inf are rejected with 400).

**Response 200** — MQTT reachable (full mode)
```json
{
  "success": true,
  "order_id": "order_1714000000123",
  "status": "full",
  "otp_code": "7429",
  "mqtt_connected": true,
  "gateway_mode": "full"
}
```

**Response 200** — MQTT unreachable (degraded mode)
```json
{
  "success": true,
  "order_id": "order_1714000000123",
  "status": "otp_only",
  "otp_code": "7429",
  "mqtt_connected": false,
  "gateway_mode": "otp_only"
}
```

OTP is always issued regardless of MQTT status. `otp_only` orders are displayed with a Wi-Fi-off badge in the Flutter UI.

**Response 500** — OTP issuance failed (crypto/rand failure — should never happen).

---

### `POST /api/orders/{id}/wake-display`

Triggers the ESP32 OLED to render the QR Code for an already-dispatched order. Called by Flutter immediately before opening `QrScannerScreen`. Idempotent — multiple calls re-render the same QR.

**Path parameter**: `id` — the order ID from the dispatch response.

**Request body**: empty (order ID is in the path; no additional parameters).

**Response 200**
```json
{ "triggered": true, "order_id": "order_1714000000123" }
```

**Response 404** — order not found or OTP already consumed.
```json
{ "error": "order not found or delivery already completed" }
```
Flutter should skip the scanner and offer manual OTP entry.

**Response 502** — MQTT broker unreachable.
```json
{ "error": "robot display is unreachable; use manual code entry" }
```
Flutter must offer manual OTP entry as fallback; must not block the user.

---

### `POST /api/validate-code`

Validates a 4-digit OTP and, on success, publishes an unlock command to the ESP32 solenoid via MQTT. Codes are single-use; concurrent validation requests for the same code are serialised under `sync.Mutex`.

**Request body**
```json
{ "code": "7429", "order_id": "order_1714000000123" }
```

Validation: `code` must be exactly 4 ASCII digit characters. `order_id` must be non-empty.

**Response 200** — unlock command delivered
```json
{ "unlocked": true, "order_id": "order_1714000000123" }
```

**Response 401** — code invalid or already consumed
```json
{ "error": "invalid or expired code" }
```
Both `ErrInvalidCode` and `ErrConsumed` map to 401 (no enumeration leakage).

**Response 502** — code consumed but MQTT publish failed
```json
{ "error": "robot is unreachable; please try again" }
```
The code is consumed even on publish failure. The customer must contact support; no replay is possible.

---

## Operator REST API (Go gateway)

All `/api/operator/*` endpoints require a staff auth token (`Authorization: Bearer <token>`). Unauthenticated requests receive `401`. These endpoints are the **only** write path into `campus_restrictions` — see CONVENTIONS.md.

**Consumed by:** the hidden `operator/` route inside the `mobile/` Flutter app (see ARCHITECTURE.md → "Known Technical Debt"). The server-side auth check below is the actual security boundary for these endpoints — the client-side route being hidden is a UX convenience, not a substitute for this check. Do not weaken or bypass this middleware under any circumstance, including for internal testing convenience.

### `POST /api/operator/auth/login`

Exchanges staff credentials for a Bearer token. Called once when a staff member enters the hidden operator route.

**Request body**
```json
{ "username": "staff_member", "password": "..." }
```

**Response 200**
```json
{ "token": "<bearer-token>", "expires_at": "2025-07-01T20:00:00Z" }
```

**Response 401** — invalid credentials.

### `GET /api/operator/deliveries?status=DISPATCHED`

Lists active robot deliveries with their most recent known telemetry point. In the current schema, delivery state is stored on `orders` and correlated by `orders.public_code` / `robot_telemetry.order_id`; there is no separate `deliveries` table.

**Query parameter**: `status` — mapped from the current order / robot lifecycle state.

**Response 200**
```json
{
  "deliveries": [
    {
      "order_id": "order_1714000000123",
      "destination_waypoint": "FT_ENTRADA",
      "current_pose": { "x": 12.35, "y": -3.42, "theta": 1.5708, "frame": "map" },
      "battery_percent": 67.5,
      "status": "NAVIGATING",
      "dispatched_at": "2025-06-30T14:02:11Z",
      "ping_count": 143
    }
  ]
}
```

### `GET /api/operator/deliveries/{order_id}/telemetry?from=<RFC3339>`

Returns the telemetry trail for an order as ROS 2 frame points plus per-point metadata, for replay/debugging. These coordinates are metres in the reported `pose.frame` (`map` from localized TF such as ORB-SLAM3 `map -> odom -> base_link`, or from an explicitly configured pose topic; otherwise `odom` fallback), not GPS longitude/latitude and not GeoJSON.

**Response 200**
```json
{
  "order_id": "order_1714000000123",
  "points": [
    {
      "timestamp": "2025-06-30T14:02:11Z",
      "pose": { "x": 12.35, "y": -3.42, "theta": 1.5708, "frame": "map" },
      "battery_percent": 78.4,
      "nav_state": "NAVIGATING",
      "remaining_m": 42.1,
      "progress_pct": 55.0
    }
  ]
}
```

### `GET /api/operator/ws/telemetry` (WebSocket)

Live subscription for delivery/telemetry state changes. On connect, the server sends the current active-delivery snapshot (same shape as `GET /api/operator/deliveries`), then pushes incremental updates as the telemetry batching worker flushes. Reconnect is client-driven — the operator route must implement exponential backoff and re-request the snapshot on reconnect; it must not assume no updates were missed during a disconnect window.

### `GET /api/operator/campus/restrictions?bounds=west,south,east,north`

Returns campus restriction zones intersecting the given bounding box, as GeoJSON.

**Response 200**
```json
{
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "geometry": { "type": "Polygon", "coordinates": [[[-47.87,-15.764],[-47.869,-15.764],[-47.869,-15.763],[-47.87,-15.763],[-47.87,-15.764]]] },
      "properties": {
        "id": 12,
        "name": "ICC construction closure",
        "restriction_type": "NO_ENTRY",
        "active_hours": null
      }
    }
  ]
}
```

Consumed by two distinct clients: (1) the onboard notebook's nav2 process, polling every 5 minutes for costmap overlay refresh (see STATE_FLOW.md), and (2) the operator route's map view, for zone visualization. Both are read-only against this endpoint.

### `POST /api/operator/campus/restrictions`

Creates a new restriction zone. Staff-authenticated. **This is the only path by which `campus_restrictions` is ever written — never from the MQTT telemetry consumer.**

**Request body**
```json
{
  "name": "ICC construction closure",
  "restriction_type": "NO_ENTRY",
  "geometry": { "type": "Polygon", "coordinates": [[...]] },
  "active_hours": null
}
```

**Response 201**
```json
{ "id": 12, "name": "ICC construction closure" }
```

**Response 400** — invalid geometry (self-intersecting polygon, wrong SRID assumption, etc.)

---

## MQTT topics

Broker: `tcp://<EC2_IP>:1883` · Authentication: M2M credentials (configured via `setup_mosquitto.sh`) · No anonymous access.

| Topic | Direction | QoS | Publisher | Subscriber(s) |
|---|---|---|---|---|
| `robot/commands/navigate` | cloud → robot | 1 | Go gateway | Onboard notebook (ROS 2) |
| `robot/commands/display_qr` | cloud → robot | 1 | Go gateway | ESP32 |
| `robot/commands/unlock` | cloud → robot | 1 | Go gateway | ESP32 |
| `robot/status/heartbeat` | robot → cloud | 1 | Onboard notebook, ESP32 | Go gateway (state/fault authority) |
| `robot/telemetry` | robot → cloud | 0 | Onboard notebook | Go gateway (batched telemetry ingestion) |

**Reliability note:** `robot/telemetry` is intentionally QoS 0 — it is a high-frequency, loss-tolerant pose stream, and the gateway's batch ingestion is designed to tolerate gaps and out-of-order delivery (see CONVENTIONS.md, batching worker). Robot **fault detection** does not depend on this stream — it depends on `robot/status/heartbeat` at QoS 1, which is the reliability-critical channel. Do not attempt to detect `FAULT` state from telemetry payloads; use the heartbeat's status field.

**Hardware note:** these topics and their payload schemas are unchanged by the Raspberry Pi → x86 notebook hardware pivot. The onboard notebook runs the identical edge daemon codebase; only the underlying compute hardware changed.

### `robot/commands/navigate` payload

Published by `OrderService.Dispatch` on a successful order.

```json
{
  "order_id": "order_1714000000123",
  "destination": { "x": 12.0, "y": -3.5 },
  "issued_at": 1714000000
}
```

### `robot/commands/display_qr` payload

Published by `WakeDisplayService.WakeDisplay` when the customer taps "Scan" in Flutter.

```json
{
  "order_id": "order_1714000000123",
  "otp": "7429",
  "issued_at": 1714000000
}
```

ESP32 firmware validates: `otp` must be exactly 4 ASCII digit characters. Stores `order_id` in `_pendingOrderId` for unlock cross-validation.

### `robot/commands/unlock` payload

Published by `OTPService.ValidateAndUnlock` after successful OTP validation.

```json
{
  "order_id": "order_1714000000123",
  "code": "7429",
  "issued_at": 1714000000
}
```

ESP32 firmware rejects the command if `order_id` does not match `_pendingOrderId` (MFA sequencing invariant).

### `robot/status/heartbeat` payload

Published every 30 s by the ESP32, and by the edge daemon on the onboard notebook. Also used as LWT (`{"source":"esp32","status":"offline"}` / `{"source":"onboard","status":"offline"}`).

```json
{
  "source": "esp32",
  "status": "online",
  "uptime_s": 3600,
  "rssi_dbm": -62,
  "free_heap": 218432,
  "actuator_armed": false,
  "display_ready": true,
  "pending_order": "order_1714000000123"
}
```

`pending_order` is `""` when idle. `display_ready` is `false` if the SSD1306 failed I2C initialisation (robot can still operate — unlock via manual OTP still works). The onboard notebook's heartbeat payload carries `status` values from `RobotState` (`IDLE`, `NAVIGATING`, `COMPLETE`, `FAULT`, `OFFLINE_HOLD`) — this is the authoritative source for robot delivery status transitions reflected on `orders`.

### `robot/telemetry` payload

Published at the configured telemetry rate by the edge daemon (onboard notebook). The current daemon publishes in all robot states; the gateway stores only payloads with a non-empty `active_order_id` and skips idle ticks. Consumed by the Go gateway's telemetry ingestion service (see CONVENTIONS.md) and written to `robot_telemetry`.

```json
{
  "source": "edge_daemon",
  "timestamp": 1714000000,
  "pose": { "x": 12.35, "y": -3.42, "theta": 1.5708, "frame": "map" },
  "velocity": { "linear_mps": 0.42 },
  "battery": { "percent": 67.5, "voltage_v": 24.2 },
  "nav_state": "NAVIGATING",
  "active_order_id": "order_1714000000123",
  "remaining_m": 42.1,
  "progress_pct": 55.0,
  "avg_speed_mps": 0.39,
  "eta_seconds": 108.0,
  "cpu_pct": 12.3,
  "mem_pct": 44.8
}
```

`active_order_id` is the same opaque order code used by dispatch (`orders.public_code` / `OTPRecord.OrderID`), not a UUID from a separate deliveries table. The pose fields are ROS 2 frame coordinates in metres; `pose.frame` is `map` when derived from localized TF (`map -> base_link`, typically via ORB-SLAM3 publishing `map -> odom`) or from an explicitly configured localized pose topic, and `odom` when falling back to `/odom`. They are not GPS longitude/latitude. `nav_state` mirrors `RobotState` but is advisory only for display purposes — `FAULT` detection is sourced from `robot/status/heartbeat`, not this topic (QoS 0 makes it unsuitable as the sole source of a safety-relevant signal). `battery.percent` refers to the **robot's traction battery** as reported by `/battery_state`, not the onboard notebook's own battery — see CONVENTIONS.md.

---

## Flutter sealed result types

### Mobile app — customer screens (`mobile/lib/`)

All API calls return sealed classes — `switch` is exhaustive at compile time.

| Method | Return type | Variants |
|---|---|---|
| `dispatchOrder()` | `DispatchResult?` | `null` on network error |
| `wakeDisplay()` | `WakeDisplayResult` | `WakeDisplayTriggered`, `WakeDisplayNotFound`, `WakeDisplayUnreachable`, `WakeDisplayNetworkError` |
| `validateOtp()` | `UnlockResult` | `UnlockSuccess`, `UnlockInvalidCode`, `UnlockRobotUnreachable`, `UnlockNetworkError` |

### Mobile app — operator route (`mobile/lib/operator/`)

Same pattern, applied to the operator API surface, isolated within its own subtree (see CONVENTIONS.md).

| Method | Return type | Variants |
|---|---|---|
| `staffLogin()` | `StaffAuthResult` | `StaffAuthSuccess`, `StaffAuthInvalidCredentials`, `StaffAuthNetworkError` |
| `listActiveDeliveries()` | `DeliveryListResult` | `DeliveryListLoaded`, `DeliveryListUnauthorized`, `DeliveryListNetworkError` |
| `getDeliveryTelemetry()` | `TelemetryReplayResult` | `TelemetryReplayLoaded`, `TelemetryReplayNotFound`, `TelemetryReplayNetworkError` |
| `createRestriction()` | `RestrictionWriteResult` | `RestrictionCreated`, `RestrictionInvalidGeometry`, `RestrictionUnauthorized`, `RestrictionNetworkError` |
| `connectTelemetryStream()` | `WebSocketConnectionState` (stream) | `Connecting`, `Connected`, `Disconnected(reason)`, `Reconnecting(attempt)` |
