# UnBot Delivery — State Flows & Invariants

## On-demand optical MFA — full sequence

This is the primary delivery sequence introduced in Phase 1.5. The QR Code is rendered lazily (when the customer initiates the scan), not eagerly (at dispatch time).

```mermaid
sequenceDiagram
    actor Customer
    participant App as Flutter app
    participant GW as Go gateway
    participant MQ as Mosquitto broker
    participant ESP as ESP32 (lock + OLED)
    participant NB as Onboard notebook (ROS 2)

    Customer->>App: Selects restaurant + confirms order
    App->>GW: POST /api/orders/{id}/dispatch
    GW->>GW: IssueOTP() → store in memory
    GW->>MQ: PUBLISH robot/commands/navigate (QoS 1)
    MQ->>NB: DELIVER navigate payload
    NB-->>NB: nav2 starts routing
    GW-->>App: 200 {otp_code, gateway_mode}
    App->>App: addOrder() → navigate to TrackingScreen

    Note over Customer,App: Robot travels to delivery address

    Customer->>App: Taps "Scan robot & open"
    App->>App: setState(_isValidating = true)
    App->>GW: POST /api/orders/{id}/wake-display
    GW->>GW: LookupByOrderID() → retrieve OTP
    GW->>MQ: PUBLISH robot/commands/display_qr (QoS 1)
    MQ->>ESP: DELIVER display_qr payload
    ESP->>ESP: Validate OTP format (4 digits)
    ESP->>ESP: Store _pendingOrderId
    ESP->>ESP: showQrCode() → render on OLED (~50 ms)
    GW-->>App: 200 {triggered: true}
    App->>App: push QrScannerScreen (~200 ms after GW response)

    Note over Customer,ESP: OLED is ready before camera focuses

    Customer->>App: Scans QR Code on OLED
    App->>App: _onDetect() validates rawValue == expectedCode
    App->>GW: POST /api/validate-code {code, order_id}
    GW->>GW: ValidateAndUnlock() — acquire mutex
    GW->>GW: Mark code as Consumed
    GW->>GW: Release mutex
    GW->>MQ: PUBLISH robot/commands/unlock (QoS 1)
    MQ->>ESP: DELIVER unlock payload
    ESP->>ESP: Cross-validate order_id == _pendingOrderId
    ESP->>ESP: showUnlockSuccess() → display ✓
    ESP->>ESP: GPIO 2 HIGH → solenoid fires (5 000 ms hold)
    ESP->>ESP: Clear _pendingOrderId
    GW-->>App: 200 {unlocked: true}
    App->>App: removeOrder(orderId, reason: 'completed')
    App->>App: setState(_codeUsed = true)
```

---

## Degraded mode — MQTT unreachable at dispatch

```mermaid
sequenceDiagram
    participant App as Flutter app
    participant GW as Go gateway
    participant MQ as Mosquitto broker

    App->>GW: POST /api/orders/{id}/dispatch
    GW->>GW: IssueOTP() → success
    GW->>MQ: PUBLISH robot/commands/navigate
    MQ--xGW: publish timeout / connection lost
    GW-->>App: 200 {gateway_mode: "otp_only", mqtt_connected: false}
    App->>App: addOrder() — isOtpOnly = true
    App->>App: Show Wi-Fi-off badge on order card
    Note over App: Customer can still validate OTP manually<br/>Robot will receive navigate when connection restores
```

---

## Wake-display failure paths

```mermaid
sequenceDiagram
    participant App as Flutter app
    participant GW as Go gateway

    App->>GW: POST /api/orders/{id}/wake-display

    alt Order not found / OTP consumed
        GW-->>App: 404 {error}
        App->>App: Show error snackbar
        App->>App: setState(_isValidating = false)
        Note over App: Manual OTP entry remains available
    else MQTT unreachable
        GW-->>App: 502 {error}
        App->>App: Show _showWakeFailureDialog()
        App->>App: setState(_isValidating = false)
        Note over App: Dialog offers "Use manual code" or "Retry"
    else Network error / timeout
        GW--xApp: TimeoutException / socket error
        App->>App: WakeDisplayNetworkError returned
        App->>App: Show _showWakeFailureDialog()
        App->>App: setState(_isValidating = false)
    end
```

---

## Telemetry ingestion — MQTT to `robot_telemetry`

This flow is deliberately decoupled at the goroutine level to satisfy the non-blocking MQTT invariant (CONVENTIONS.md). The Paho callback and the Postgres write never share a call stack.

```mermaid
sequenceDiagram
    participant NB as Onboard notebook (edge daemon)
    participant MQ as Mosquitto broker
    participant CB as Go gateway — Paho OnMessage callback
    participant CH as Buffered channel (cap 500)
    participant BW as Batching worker goroutine
    participant PG as PostgreSQL (robot_telemetry)

    loop Every telemetry tick
        NB->>MQ: PUBLISH robot/telemetry (QoS 0)
    end
    MQ->>CB: DELIVER telemetry payload
    CB->>CB: json.Unmarshal(payload)
    alt Payload valid
        alt active_order_id is empty
            CB->>CB: skippedCounter.Add(1)
            Note over CB: Idle telemetry is observed but not persisted
        else active_order_id is present
            CB->>CH: non-blocking send
            alt Channel has capacity
                CH-->>CB: accepted
            else Channel full
                CB->>CB: droppedCounter.Add(1)
                Note over CB: Callback returns immediately either way —<br/>never blocks on a full channel
            end
        end
    else Payload malformed
        CB->>CB: log warning, discard
    end

    par Batching worker loop (independent goroutine)
        BW->>CH: receive telemetry point
        BW->>BW: append to in-memory batch
        alt batch size >= 50 OR 2s elapsed since last flush
            BW->>PG: InsertBatch() — multi-row INSERT
            PG-->>BW: ack / error (logged, non-fatal)
            BW->>BW: reset batch
        end
    end
```

**Invariants:**
- The Paho callback (`CB`) never calls into `PG` directly, under any code path, including error handling.
- Channel overflow drops the telemetry point being sent (Go's `select`/`default` pattern does not evict queued points). For `robot_telemetry` this is acceptable data loss; it is never acceptable for order/status-transition messages, which must use a separate, higher-priority path if the same batching architecture is reused for them.
- A failed `InsertBatch()` is logged and the batch is discarded, not retried indefinitely — telemetry is best-effort observability data, not transactional state. Retrying indefinitely against a degraded Postgres would itself become a backpressure source into the channel.
- `robot/status/heartbeat` (QoS 1, delivery status / fault authority) is **not** subject to this drop policy — heartbeat-driven status transitions reflected on `orders` use a dedicated, higher-priority path that is allowed to block briefly (bounded by a short timeout) rather than silently drop a `FAULT` transition.
- This flow is unaffected by the Raspberry Pi → x86 notebook hardware pivot — only the participant label changed.

---

## Campus restriction refresh — nav2 polling with cached fallback

nav2 (running on the onboard notebook) never holds a live database or MQTT subscription to `campus_restrictions`. It polls the Gateway's REST endpoint and degrades to cache on failure — a stale restriction set is preferable to blocking navigation on a network call.

```mermaid
sequenceDiagram
    participant NAV as nav2 (onboard notebook)
    participant GW as Go gateway
    participant PG as PostGIS (campus_restrictions)
    participant FS as Local disk cache (onboard notebook)

    loop Every 5 minutes
        NAV->>GW: GET /api/operator/campus/restrictions?bounds=<campus_bbox>
        alt Success
            GW->>PG: SELECT ... WHERE ST_Intersects(geometry, bbox)
            PG-->>GW: restriction rows
            GW-->>NAV: 200 GeoJSON FeatureCollection
            NAV->>FS: overwrite cached restriction set
            NAV->>NAV: rebuild costmap overlay from new set
        else Network failure / timeout
            NAV->>FS: read last-cached restriction set
            NAV->>NAV: continue operating on cached costmap overlay
            Note over NAV: A stale-but-known restriction set is safer<br/>than blocking navigation on a failed poll
        end
    end
```

**Invariant:** nav2's navigation loop never blocks on this poll. The poll runs on its own timer, independent of the navigation goal execution loop, and a poll failure is logged but never propagated as a navigation fault. **This flow is unaffected by the hardware pivot** — the x86 notebook's larger local disk is, if anything, more forgiving for the cache file than the Pi's SD card would have been.

---

## Operator staff authentication — login flow (NEW)

Precedes any `/api/operator/*` call other than `POST /api/operator/auth/login` itself. Runs inside the hidden `mobile/lib/operator/` route.

```mermaid
sequenceDiagram
    actor Staff as Ops staff
    participant OPS as mobile/lib/operator/ route
    participant GW as Go gateway

    Staff->>OPS: Trigger hidden entry point (long-press / deep link)
    OPS->>OPS: Show staff login form
    Staff->>OPS: Enter username + password
    OPS->>GW: POST /api/operator/auth/login {username, password}
    alt Invalid credentials
        GW-->>OPS: 401
        OPS->>OPS: Show error, remain on login form
    else Valid credentials
        GW-->>OPS: 200 {token, expires_at}
        OPS->>OPS: staffAuthTokenNotifier.value = token
        Note over OPS: Token stored in operator/-scoped state only —<br/>never in customer-facing ValueNotifiers, never hardcoded
        OPS->>OPS: Navigate to operator dashboard home
    end

    Note over OPS,GW: Every subsequent /api/operator/* call attaches<br/>Authorization: Bearer <token>. The hidden route entry<br/>point is UX only — this token check is the real boundary.
```

---

## Operator zone authorship — REST-only write path

```mermaid
sequenceDiagram
    actor Staff as Ops staff
    participant OPS as mobile/lib/operator/ route
    participant GW as Go gateway
    participant PG as PostGIS (campus_restrictions)

    Staff->>OPS: Fill zone form (name, type, WKT/GeoJSON, active_hours)
    OPS->>GW: POST /api/operator/campus/restrictions (Bearer token)
    GW->>GW: authMiddleware validates staff token
    alt Unauthorized
        GW-->>OPS: 401
    else Authorized
        GW->>GW: validate geometry (well-formed, non-self-intersecting)
        alt Invalid geometry
            GW-->>OPS: 400 {error}
        else Valid
            GW->>PG: INSERT INTO campus_restrictions (...)
            PG-->>GW: new row id
            GW-->>OPS: 201 {id, name}
            OPS->>OPS: append zone to map layer, no page reload
        end
    end

    Note over GW,PG: This is the ONLY write path into campus_restrictions.<br/>The MQTT telemetry subscriber never writes to this table —<br/>see CONVENTIONS.md "campus_restrictions writes — REST path only"
```

---

## Operator E-stop - Nav2 cancel flow

```mermaid
sequenceDiagram
    actor Staff as Ops staff
    participant OPS as mobile/lib/operator route
    participant GW as Go gateway
    participant MQ as Mosquitto broker
    participant NB as Onboard notebook
    participant NAV as NavBridge / Nav2
    participant SM as RobotStateMachine

    Staff->>OPS: Press emergency stop
    OPS->>GW: POST /api/robot/estop
    GW->>MQ: PUBLISH robot/commands/estop (QoS 2)
    MQ->>NB: DELIVER estop payload
    NB->>NB: MQTTBridge enqueues estop_queue
    NB->>NAV: estop_observer calls cancel_current_goal()
    NAV->>NAV: goal_handle.cancel_goal_async()
    NAV-->>NB: cancel response / timeout logged
    NB->>SM: trigger_fault("estop: <reason>")
    Note over NAV,SM: FAULT state alone is not the stop mechanism.<br/>The active Nav2 action goal must be cancelled first.
```

**Invariant:** ESTOP received while `NAVIGATING` must cancel the active Nav2 action goal before or alongside the `FAULT` state transition. Do not reintroduce direct `trigger_fault()` calls from the MQTT dispatch path; ESTOP flows through `estop_queue` so the nav bridge can cancel the physical motion command.

---

## Go OTP service — state invariants

```
OTPRecord.Consumed transitions: false → true (one-way, irreversible)

ValidateAndUnlock critical section:
  1. Acquire mu
  2. Look up code → if absent: release mu, return ErrInvalidCode
  3. If Consumed: release mu, return ErrConsumed
  4. Set Consumed = true
  5. Release mu          ← code is consumed before MQTT publish
  6. Publish unlock
  7. If publish fails: return ErrPublish (code already consumed — no replay)

Invariant: a code can open exactly one compartment, regardless of
concurrent requests, MQTT failures, or client retries.
```

---

## `orders` robot status — state invariants

```
orders robot lifecycle transitions: pending/placed → dispatched → navigating → (completed | failed/cancelled)

Sourced from robot/status/heartbeat (QoS 1), not robot/telemetry (QoS 0).
Enforced one-way at the schema level — mirrors OTPRecord.Consumed.

Once DELIVERED or FAILED, no further transition is accepted; a heartbeat
arriving after terminal state is logged as an anomaly (e.g., ESP32/onboard
notebook clock drift or duplicate delivery) and discarded, not applied.
```

---

## ESP32 connection state machine

```mermaid
stateDiagram-v2
    [*] --> CS_BOOT
    CS_BOOT --> CS_WIFI_CONNECTING : begin()
    CS_WIFI_CONNECTING --> CS_WIFI_CONNECTING : timeout → restart WiFi.begin()
    CS_WIFI_CONNECTING --> CS_WIFI_CONNECTED : WL_CONNECTED
    CS_WIFI_CONNECTED --> CS_MQTT_CONNECTING : immediate (next tick)
    CS_MQTT_CONNECTING --> CS_MQTT_CONNECTING : connect fail → exponential backoff (2s→60s)
    CS_MQTT_CONNECTING --> CS_MQTT_CONNECTED : client.connected()
    CS_MQTT_CONNECTED --> CS_WIFI_LOST : WiFi.status() ≠ WL_CONNECTED
    CS_MQTT_CONNECTED --> CS_MQTT_LOST : !client.connected()
    CS_WIFI_LOST --> CS_WIFI_CONNECTING : startWifi()
    CS_MQTT_LOST --> CS_MQTT_CONNECTING : attemptMqttConnect()
```

**Key invariant**: `client.loop()` is called **only** in `CS_MQTT_CONNECTED`. Calling it in any other state reads from a null/stale TCP socket and can trigger a hard fault on ESP-IDF.

---

## Flutter `ValueNotifier` mutation rules (mobile app — customer screens)

All three global notifiers follow the same immutable-swap protocol:

```
// CORRECT — triggers listeners
activeOrdersNotifier.value = [...current, newOrder];

// WRONG — mutates list in place, listeners DO NOT fire
activeOrdersNotifier.value.add(newOrder);  // ← NEVER DO THIS
```

`removeOrder()` is atomic from the UI's perspective:
1. Find departing order in `activeOrdersNotifier.value`
2. Call `archivePastOrder()` → prepend to `pastOrdersNotifier.value`
3. Write filtered list to `activeOrdersNotifier.value`

Both notifiers fire in the same synchronous call stack. No frame exists where an order is absent from both lists simultaneously.

**The `operator/` route's notifiers (`activeDeliveriesNotifier`, `staffAuthTokenNotifier`) follow the same immutable-swap discipline but are declared and scoped entirely within `mobile/lib/operator/` — see CONVENTIONS.md.**

---

## Active order lifecycle (mobile app — customer screens)

```
Placement          Tracking           Pickup              Archive
─────────          ────────           ──────              ───────
addOrder()    →    activeOrdersNotifier  →  removeOrder(    →  pastOrdersNotifier
                   isOtpOnly badge          reason: 'completed'   reason badge:
                   TrackingScreen           or 'cancelled')       'Entregue' / 'Cancelado'
```

`reason: 'completed'` is set by `code_screen.dart` (OTP validated).  
`reason: 'cancelled'` is set by the cancel dialog in `tracking_screen.dart`.  
Default parameter on `removeOrder()` is `'completed'` — callers at non-happy-path sites must be **explicit**.

---

## Edge daemon RobotStateMachine — delivery lifecycle

Runs on the onboard x86 notebook. Unaffected in logic by the hardware pivot from Raspberry Pi.

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> NAVIGATING : receive robot/commands/navigate
    NAVIGATING --> NAVIGATING : publish telemetry at 1 Hz
    NAVIGATING --> COMPLETE : receive robot/commands/unlock (from unlock_observer)
    NAVIGATING --> OFFLINE_HOLD : lose MQTT connection
    OFFLINE_HOLD --> NAVIGATING : MQTT reconnects (resume active goal)
    OFFLINE_HOLD --> FAULT : exceed offline_timeout (safety)
    NAVIGATING --> FAULT : nav2 signals unrecoverable error
    COMPLETE --> IDLE : clear active goal, mark delivery terminal
    FAULT --> IDLE : human intervention / reset command

    Note over IDLE,FAULT: Current daemon publishes telemetry in all states;<br/>gateway persists only non-empty active_order_id ticks.<br/>Heartbeat publishes every 30s in all states.<br/>Robot delivery status is reflected on `orders` via heartbeat (QoS 1),<br/>not via telemetry (QoS 0).
```

---

## Operator route WebSocket subscription lifecycle

```mermaid
sequenceDiagram
    participant OPS as mobile/lib/operator/ route
    participant GW as Go gateway
    participant WS as WebSocket connection

    OPS->>GW: GET /api/operator/ws/telemetry (WebSocket handshake, Bearer token)
    GW->>GW: authMiddleware validates staff token
    alt Unauthorized
        GW-->>OPS: 401 Unauthorized
    else Authorized
        GW->>GW: Accept WebSocket upgrade
        GW->>GW: Send current active-delivery snapshot
        GW-->>OPS: {deliveries: [...]}
        Note over OPS,GW: WebSocket is now open

        loop Live updates (bounded by batching worker flush interval)
            par On telemetry tick
                GW->>GW: batch worker flushes N points to DB
                GW->>WS: push incremental delivery update
                WS-->>OPS: {order_id, current_pose, battery, ...}
                OPS->>OPS: animate marker, update card
            end
        end

        alt Client loses connection
            WS--xOPS: disconnect / network error
            OPS->>OPS: exponential backoff, reconnect timer
            OPS->>GW: GET /api/operator/ws/telemetry (new handshake)
            GW-->>OPS: send full snapshot again
            Note over OPS: No updates were queued during disconnect.<br/>Re-snapshot ensures consistency.
        else Client explicitly closes
            OPS->>WS: close frame
        end
    end
```

---

## Data model — PostgreSQL schema

### `robot_telemetry` table

```sql
CREATE TABLE robot_telemetry (
  id                 bigserial PRIMARY KEY,
  order_id           text NOT NULL,
  ts                 timestamptz NOT NULL,
  nav_state          text NOT NULL,
  pose_x             double precision,
  pose_y             double precision,
  pose_theta         double precision,
  map_frame          text,
  linear_speed_mps   double precision,
  avg_speed_mps      double precision,
  battery_percent    double precision,   -- robot traction battery, NOT the onboard notebook's own battery
  battery_voltage_v  double precision,
  remaining_m        double precision,
  progress_pct       double precision,
  eta_seconds        double precision,
  cpu_pct            double precision,
  mem_pct            double precision,
  created_at         timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX idx_robot_telemetry_order_ts ON robot_telemetry(order_id, ts);
CREATE INDEX idx_robot_telemetry_ts ON robot_telemetry(ts);
```

**Schema notes:**
- `order_id` is the opaque public order code (`orders.public_code`, `OTPRecord.OrderID`, dispatch path parameter). It is deliberately **not** a foreign key: mock/test order IDs must not poison a real `pgx.CopyFrom` batch.
- `pose_x`, `pose_y`, and `pose_theta` are ROS 2 frame coordinates in metres. `map_frame` records whether the point came from localized TF (`map -> base_link`, typically via ORB-SLAM3 `map -> odom -> base_link`), an explicitly configured localized pose topic, or odometry fallback (`/odom`). Do not store them as PostGIS SRID 4326 geometry without a separate calibrated frame -> GPS transform.
- If onboard compute battery state is needed later (distinct from robot traction battery), add a separate `compute_battery_percent` column rather than overloading `battery_percent` — see ARCHITECTURE.md → "Hardware topology" for the rationale.

### `orders` robot-delivery fields

```sql
CREATE TABLE orders (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  public_code text NOT NULL UNIQUE,
  client_user_id uuid NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
  restaurant_id uuid NOT NULL REFERENCES restaurants(id) ON DELETE RESTRICT,
  delivery_address text NOT NULL,
  status order_status NOT NULL DEFAULT 'pending',
  subtotal_cents integer NOT NULL CHECK (subtotal_cents >= 0),
  delivery_fee_cents integer NOT NULL DEFAULT 0 CHECK (delivery_fee_cents >= 0),
  discount_cents integer NOT NULL DEFAULT 0 CHECK (discount_cents >= 0),
  total_cents integer NOT NULL CHECK (total_cents >= 0),
  robot_dispatched boolean NOT NULL DEFAULT false,
  gateway_mode gateway_mode,
  mqtt_connected boolean NOT NULL DEFAULT false,
  placed_at timestamptz NOT NULL DEFAULT now(),
  dispatched_at timestamptz,
  completed_at timestamptz,
  cancelled_at timestamptz,
  cancel_reason text,
  notes text,
  created_at timestamptz NOT NULL DEFAULT now(),
  updated_at timestamptz NOT NULL DEFAULT now(),
  CHECK (total_cents = subtotal_cents + delivery_fee_cents - discount_cents)
);

CREATE INDEX idx_orders_client ON orders(client_user_id, placed_at DESC);
CREATE INDEX idx_orders_restaurant ON orders(restaurant_id, placed_at DESC);
CREATE INDEX idx_orders_status ON orders(status);
```

There is no separate `deliveries` table in the current codebase. Robot dispatch and delivery metadata were added directly to `orders`; telemetry correlates to it by `orders.public_code`.

### `campus_restrictions` table

```sql
CREATE TABLE campus_restrictions (
  id SERIAL PRIMARY KEY,
  name VARCHAR(256),
  geometry GEOMETRY(POLYGON, 4326),
  restriction_type VARCHAR(32),
  active_hours TSRANGE,
  created_by UUID,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  
  CHECK (restriction_type IN ('NO_ENTRY', 'SLOW_ZONE', 'INDOOR_ONLY'))
);

CREATE INDEX idx_campus_restrictions_geometry 
  ON campus_restrictions USING GIST(geometry);
```

All campus restriction geometry columns use `SRID 4326` (WGS 84 lat/lon). `GIST` indices are mandatory for spatial queries. This rule applies to geographic campus restriction zones, not to ROS 2 frame-based telemetry.

### `staff_users` table (NEW — required by operator auth flow)

```sql
CREATE TABLE staff_users (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(256) NOT NULL,   -- bcrypt/argon2, never plaintext
  created_at TIMESTAMPTZ DEFAULT NOW()
);
```

Backing store for `POST /api/operator/auth/login` (see PROTOCOL.md). Token issuance/validation logic (JWT or opaque token + session table) is an implementation detail left to the Go gateway's auth middleware — not specified further here, but must not be skipped or stubbed out "temporarily" for the milestone, since this table backs the only real access control on `/api/operator/*`.
