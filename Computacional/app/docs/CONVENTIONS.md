# UnBot Delivery — Engineering Conventions

## ESP32 firmware (C++ / Arduino)

### No heap allocation after `setup()`

All data structures must be stack-allocated or static. Do not call `new`, `malloc`, `String`, or any STL container that allocates on the heap after the first loop iteration. Heap fragmentation on the 520 KB SRAM will cause silent OOM failures hours into a demo.

```cpp
// CORRECT
uint8_t qrData[qrcode_getBufferSize(QR_VERSION)];  // stack
static char _pendingOrderId[64];                    // static

// WRONG
String otp = String(otpCode);   // heap allocation
std::vector<uint8_t> buf;       // heap allocation
```

### No blocking calls in `loop()`

`loop()` must return in microseconds. Never call `delay()`, `WiFi.begin()` synchronously with a wait, or any function that blocks. Use `millis()`-based timers for all deferred work. The `MqttManager` state machine is the reference pattern.

### `client.loop()` guard

Only call `_client.loop()` when `_state == CS_MQTT_CONNECTED`. Calling it in any other state reads from a stale TCP socket. The check is enforced in `_handleMqttConnected()` — do not bypass it.

### No C++ digit separators

The Arduino toolchain on ESP32 does not support C++14 digit separators (`15'000`). Write `15000`. Violations will cause a silent compile error that is hard to diagnose.

### `ConnectionState` enum prefix

All enum values carry the `CS_` prefix to avoid collision with PubSubClient preprocessor macros (`MQTT_CONNECTED`, `MQTT_DISCONNECTED`, etc.). Never add a new `ConnectionState` value without the prefix.

### MFA sequencing — `_pendingOrderId` invariant

`onUnlock()` **must** reject any unlock command where `_pendingOrderId` is empty or does not match the incoming `order_id`. This is the firmware's contribution to the MFA guarantee. Do not remove or weaken this check.

```cpp
// MANDATORY — do not remove
if (_pendingOrderId[0] == '\0') { displayMgr.showError("NO QR DISPLAYED"); return; }
if (strncmp(_pendingOrderId, orderId, sizeof(_pendingOrderId)) != 0) { displayMgr.showError("ORDER MISMATCH"); return; }
```

---

## Edge daemon (Python / asyncio) — runs on the onboard x86 notebook

### No database credentials, ever

The edge daemon must never import a Postgres/PostGIS driver, hold a connection string, or receive one via environment variable, config file, or MQTT payload. Every piece of data that must reach PostGIS travels exclusively through the `robot/telemetry` and `robot/status/heartbeat` MQTT topics, consumed by the Go gateway. This is enforced by code review, not by runtime checks — if a PR to `edge_daemon/` imports `asyncpg`, `psycopg`, or similar, reject it regardless of justification. **This invariant is unaffected by the Raspberry Pi → x86 notebook hardware change** — it is a security boundary, not a hardware constraint.

### Stub mode — unchanged by the hardware pivot

`edge_daemon`'s existing `rclpy` import check at startup still governs stub vs. full mode. On the onboard x86 notebook (full ROS 2 desktop distro installed), this resolves to full mode automatically — no code change required. Developer laptops without ROS 2 installed continue to run in stub mode exactly as before. Do not add hardware-specific branches (e.g. `if platform == 'raspberry_pi'`) — the daemon should remain agnostic to the underlying compute platform.

### Monotonic timestamps on telemetry payloads

Each `robot/telemetry` publish must carry a timestamp derived from a single monotonic source read once per publish cycle, not re-derived per field. Out-of-order delivery is tolerated at the transport layer (QoS 0), but the payload's own timestamp must be trustworthy enough to reconstruct delivery order on replay.

### Telemetry publish frequency

Publish `robot/telemetry` at 1 Hz during `NAVIGATING` state. Do not increase this without first confirming Mosquitto broker CPU and the Go gateway's batch-insert throughput can absorb it — see the Go gateway telemetry ingestion pattern below before changing this constant.

### Battery telemetry — scope `battery_percent` explicitly

The onboard x86 notebook has its own battery, separate from the robot's traction/chassis battery. `robot_telemetry.battery_percent` refers to the **robot's main traction battery** — this is the operationally relevant value for delivery range and E-stop decisions. If notebook compute battery state needs tracking (e.g., to anticipate a compute shutdown independent of robot mobility), publish it as a **separate field**, never overloaded into `battery_percent`.

---

## Go gateway

### Dependency injection via constructor parameters

Services receive their dependencies through constructor functions, never via package-level globals or `init()`. This keeps tests deterministic and services independently testable.

```go
// CORRECT
otpSvc := services.NewOTPService(mqttClient)
orderSvc := services.NewOrderService(otpSvc, mqttClient, log)

// WRONG
var globalOtpSvc *services.OTPService  // ← never
```

### Handlers have zero business logic

HTTP handlers in `internal/api/` are translation layers only: decode → validate inputs → call service → map typed errors to HTTP status codes → encode. Business logic lives in `internal/services/`.

### Typed errors — no sentinel strings

Service functions return typed sentinel errors (`var ErrInvalidCode = fmt.Errorf(...)`) wrapped with `fmt.Errorf("%w: ...", ErrInvalidCode, ...)`. Handlers use `errors.Is()` to map them. Never compare `err.Error()` strings.

### OTP code consumed before MQTT publish

In `ValidateAndUnlock`, the `Consumed = true` flag is set and the mutex is released **before** calling `publisher.Publish()`. This ensures the code cannot be replayed even if the publish fails or the process crashes mid-publish.

### `sync.Mutex` — never hold across network I/O

The mutex in `OTPService` protects only the in-memory store mutation. Release it before calling `publisher.Publish()` (a network operation). Holding a mutex across I/O is a latency trap and can deadlock if the MQTT client calls back into a goroutine that tries to acquire the same lock.

### No business logic in `main.go`

`cmd/gateway/main.go` performs only: load config → construct MQTT client → construct services → construct server → start → block on signal → shutdown. No request handling, no domain logic.

### Telemetry MQTT subscriber — mandatory producer/consumer separation

The Paho `OnMessage` callback bound to `robot/telemetry` and `robot/status/heartbeat` must **never** perform a synchronous database write. It performs exactly one action: a non-blocking send into a buffered channel.

```go
// CORRECT — Paho callback stays non-blocking, drop-oldest on overflow
func (s *TelemetryIngestService) onTelemetryMessage(client mqtt.Client, msg mqtt.Message) {
    var pt TelemetryPoint
    if err := json.Unmarshal(msg.Payload(), &pt); err != nil {
        s.log.Warn("malformed telemetry payload", "err", err)
        return
    }
    select {
    case s.telemetryChan <- pt:
    default:
        s.droppedCounter.Add(1) // buffer full — record and move on, never block
    }
}

// WRONG — blocks the Paho read loop on every message
func (s *TelemetryIngestService) onTelemetryMessage(client mqtt.Client, msg mqtt.Message) {
    var pt TelemetryPoint
    json.Unmarshal(msg.Payload(), &pt)
    s.db.Exec(ctx, "INSERT INTO robot_telemetry (...) VALUES (...)", ...) // ← NEVER
}
```

The channel is drained by a dedicated worker goroutine (started once at service construction, not per-message) that batches inserts:

```go
// CORRECT — batching worker, owned by the service, independent of Paho
func (s *TelemetryIngestService) runBatcher(ctx context.Context) {
    const batchSize = 50
    const flushInterval = 2 * time.Second

    batch := make([]TelemetryPoint, 0, batchSize)
    ticker := time.NewTicker(flushInterval)
    defer ticker.Stop()

    flush := func() {
        if len(batch) == 0 {
            return
        }
        if err := s.repo.InsertBatch(ctx, batch); err != nil {
            s.log.Error("telemetry batch insert failed", "err", err, "size", len(batch))
        }
        batch = batch[:0]
    }

    for {
        select {
        case pt := <-s.telemetryChan:
            batch = append(batch, pt)
            if len(batch) >= batchSize {
                flush()
            }
        case <-ticker.C:
            flush()
        case <-ctx.Done():
            flush()
            return
        }
    }
}
```

The channel buffer size, batch size, and flush interval are named constants, not magic numbers, and are tuned against the 1 Hz publish rate defined in the edge daemon convention above. If you change the publish frequency, revisit these constants together.

### `campus_restrictions` writes — REST path only, never from the MQTT subscriber

There is no code path from `TelemetryIngestService` (or any MQTT `OnMessage` handler) into the `campus_restrictions` table. All writes to this table originate from `internal/api/operator_restrictions.go` handlers, gated by staff auth middleware, following the same handler → service → typed error pattern as every other REST endpoint. If a future feature needs the robot to report a detected obstacle, it lands in a new, separate `reported_obstacles` table — never directly in `campus_restrictions`.

### PostGIS conventions

- All geometry columns use `SRID 4326` (WGS 84 lat/lon) — never mix SRIDs across tables.
- Every geometry column has a `GIST` index. A `campus_restrictions` or `robot_telemetry` query without one will silently degrade to a sequential scan as the table grows — this is not optional.
- Batch inserts use multi-row `INSERT` or `pgx.CopyFrom`, never a loop of single-row `Exec` calls from Go — this is the direct consequence of the telemetry batching pattern above.
- Delivery status transitions are enforced one-way at the schema level with a `CHECK` constraint or a trigger, mirroring the `OTPRecord.Consumed` one-way invariant already established for OTP codes.

---

## Flutter (Dart) — mobile app (`mobile/`)

### `ValueNotifier` — always swap, never mutate

```dart
// CORRECT — listeners fire
activeOrdersNotifier.value = [...current, order];

// WRONG — listeners DO NOT fire
activeOrdersNotifier.value.add(order);  // ← NEVER
```

### UI lock (`_isValidating`) — unconditional release in `finally`

Every async chain that sets `_isValidating = true` must release it in a `finally` block. No early `return` path may skip the release — the button would be permanently disabled.

```dart
// CORRECT
Future<void> _escanearERetirar() async {
  setState(() => _isValidating = true);
  try {
    // ... async work ...
  } finally {
    if (mounted) setState(() => _isValidating = false);
  }
}
```

### Controllers — never construct in `build()`

`TextEditingController` and `AnimationController` must be created in `initState()` and disposed in `dispose()`. Constructing them inside `build()` creates a new controller on every rebuild, leaks memory, and resets cursor position.

### `removeOrder()` at non-happy-path sites must be explicit

The default `reason` parameter on `removeOrder()` is `'completed'`. Any call site that is NOT the OTP validation success path must pass `reason: 'cancelled'` explicitly. Failure produces incorrect history badges.

```dart
// Order completion (code_screen.dart)
removeOrder(widget.orderId);  // default 'completed' is correct here

// User cancellation (tracking_screen.dart)
removeOrder(order.orderId, reason: 'cancelled');  // MUST be explicit
```

### `mounted` check after every `await`

Any `setState()`, `Navigator` call, or `ScaffoldMessenger` call after an `await` must be guarded with `if (!mounted) return`. The widget may have been disposed while the async operation was in flight.

### Sealed result types — no raw status code checks

API responses are returned as sealed class hierarchies (`UnlockResult`, `WakeDisplayResult`). Use `switch` on the result type. Never check `response.statusCode` directly in widget code.

### `AC.*` context-aware color accessors — not `AppColors.*`

All widget-layer color references must use `AC.primary(context)`, `AC.card(context)`, etc. Direct `AppColors.primary` / `AppColors.card` references are hardcoded to light mode and will look broken in dark mode. `AppColors.*` is permitted only for theme-invariant values (`AppColors.accent`, `AppColors.teal`, `AppColors.statusDelivered`, etc.).

---

## Flutter (Dart) — operator route (`mobile/lib/operator/`) (NEW — folded into mobile/ as tech debt)

**This is technical debt, deliberately accepted for the 2-week milestone. See ARCHITECTURE.md → "Known Technical Debt" for the full rationale and extraction plan.** The rules below exist specifically to keep that future extraction mechanical rather than a rewrite.

### Directory isolation is mandatory

All operator code lives under `mobile/lib/operator/` — screens, controllers, API client, models. Nothing outside this directory imports from it except the single hidden route registration in the app's router. Nothing inside this directory imports the customer-facing `ValueNotifier` globals (`activeOrdersNotifier`, `userStateNotifier`, `pastOrdersNotifier`) or customer screens.

```dart
// CORRECT — operator/ has its own isolated state
// mobile/lib/operator/state/operator_state.dart
final activeDeliveriesNotifier = ValueNotifier<List<DeliverySnapshot>>([]);
final staffAuthTokenNotifier = ValueNotifier<String?>(null);

// WRONG — reaching into customer-facing state from operator/
import 'package:unbot/state/order_state.dart'; // ← NEVER inside operator/
```

### Entry point — non-discoverable, not customer-navigable

The operator route is reachable via a hidden trigger (recommended: long-press on a low-traffic element, or a deep link path not linked from any customer-facing button) — never a visible item in the customer bottom navigation or menu. **This is UX hygiene only.** The actual access control is the staff `Bearer` token required by every `/api/operator/*` call — see PROTOCOL.md. Do not treat the hidden entry point as a security measure; assume any sufficiently motivated party can find and reach this route.

### Staff auth token — never hardcoded, prompted and stored per session

The operator route prompts for staff credentials on entry and exchanges them for a Bearer token via the Gateway's auth flow. The token lives in `operator/`-scoped state (`staffAuthTokenNotifier` or equivalent), not in a compile-time constant, not committed to version control.

### Tile source — self-hosted only, never public OSM tile servers in a running build

`flutter_map`'s `TileLayer.urlTemplate` must point at the self-hosted tile cache (ARCHITECTURE.md → "Self-hosted tile cache"), scoped to the UnB campus bounding box. Pointing this at `tile.openstreetmap.org` directly is a policy violation under sustained/production use and a demo-day outage risk (rate limiting). This applies to local development too — developers pull from the same cache or a locally seeded subset, not the public server.

### Robot marker — interpolate, never snap

Telemetry arrives in discrete ticks. Setting the marker's `LatLng` directly on each tick produces visible teleportation. Use `AnimatedMapController` (or an equivalent tween) to animate the marker smoothly across the interval between the previous and current telemetry point.

```dart
// CORRECT — smooth interpolation between telemetry ticks
void _onTelemetryUpdate(LatLng newPosition) {
  final tween = LatLngTween(begin: _currentMarkerPosition, end: newPosition);
  _markerAnimController.reset();
  _markerAnimController.forward();
  // listener updates _currentMarkerPosition from tween.evaluate() each frame
}

// WRONG — visible jump on every tick
void _onTelemetryUpdate(LatLng newPosition) {
  setState(() => _currentMarkerPosition = newPosition); // teleports
}
```

### Live updates via WebSocket, not polling

The operator route subscribes to `GET /api/operator/ws/telemetry` (WebSocket) for live delivery/telemetry updates. REST polling is reserved for one-shot loads (initial delivery list, telemetry replay history) — not for the live map view, which must reflect state changes within the batching worker's flush interval, not a poll cycle.

### Campus zone editing — form-based, not map-drawn

`campus_restrictions` CRUD is a form (name, `restriction_type`, WKT/GeoJSON text field, `active_hours`), not interactive polygon drawing on the map. `flutter_map`'s drawing/editing tooling is not mature enough to justify the effort at this phase. Complex zone geometry is authored in QGIS and imported via a migration script.

---

## MQTT topics — single source of truth

Topic strings are defined **once** in `gateway/internal/services/otp.go` (`TopicUnlock`, `TopicNavigate`, `TopicDisplayQR`, `TopicTelemetry`), **once** in `hardware/esp32-lock/src/main.cpp` (`TOPIC_DISPLAY_QR`, `TOPIC_UNLOCK`, `TOPIC_HEARTBEAT`), and **once** in `edge_daemon/topics.py` (`Topics.NAVIGATE`, `Topics.TELEMETRY`, etc.). Never hardcode a topic string at a call site. If a topic changes, update all three files.

---

## Credentials — never in version control

| File | Status |
|---|---|
| `gateway/.env` | `.gitignore`'d — copy from `.env.example`. **This is the only file in the entire monorepo permitted to contain a PostGIS connection string.** |
| `hardware/esp32-lock/include/secrets.h` | `.gitignore`'d — copy from `secrets.h.example` |
| `edge_daemon/.env` | `.gitignore`'d — MQTT credentials only. Must never contain a `DATABASE_URL`, `PG*` variable, or any database credential, by convention and by code review. |

The `.example` files are the only credential artifacts that enter git. Production credentials are injected via environment variables (systemd `EnvironmentFile` on the onboard notebook, or NVS on the ESP32). **Staff auth tokens for the `operator/` route are never hardcoded in the Flutter source** — see the operator-route conventions above.

---

## Testing

### Go — table-driven tests with mock publisher

All service tests use the `mockPublisher` pattern from `otp_test.go`. Tests must cover:
- Happy path (success)
- Invalid input rejection
- Consumed/already-used code
- MQTT publish failure
- Concurrent access (run with `-race`)

Telemetry ingestion tests additionally cover:
- Batch flush on size threshold vs. time threshold
- Channel-full drop behavior does not block the caller (test with a stalled/mock repo)
- Malformed payload is logged and skipped, does not crash the batcher

### Flutter — no widget tests for screens with platform channels

`MobileScannerController` and camera APIs cannot be tested in a widget test environment. Test business logic (controllers, state helpers) as pure unit tests. Screen tests are manual / integration only. This applies to customer-facing `mobile/` screens. `operator/` screens have no camera dependency and their map/telemetry-replay logic should be unit tested where feasible (tween math, batching/reconnect logic) independent of `flutter_map`'s own rendering.
