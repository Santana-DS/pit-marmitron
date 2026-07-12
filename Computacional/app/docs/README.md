# MARMITRON 3000 — Documentation Update (V2.3, Milestone Pivot)

## What changed since V2.2

Two constraints came from the team, both accepted with conditions attached. This version of the docs reflects both.

### 1. Operator dashboard folded into `mobile/` (technical debt, deliberately accepted)

**Why:** 2-week milestone deadline. A separate Flutter Web package (`operator/`) with its own deployment pipeline was disproportionate to the timeline.

**What this means practically:**
- No new package, no new `pubspec.yaml`, no new nginx container.
- Operator screens live at `mobile/lib/operator/` — a self-contained subtree within the existing app.
- Reachable via a **hidden, non-discoverable entry point** (long-press gesture or obscure deep link) — never a customer-facing nav item.

**The condition attached, and the one thing you cannot skip:** the hidden route is UX convenience, not security. The real access control is the staff `Bearer` token required server-side on every `/api/operator/*` call. If your team is tempted to skip implementing `POST /api/operator/auth/login` and `staff_users` "just for the demo," don't — that table and that check are the only thing standing between "staff-only dashboard" and "anyone who finds the route can see and edit robot navigation zones." See ARCHITECTURE.md → "Known Technical Debt" and STATE_FLOW.md → "Operator staff authentication."

**What you get for free later:** because all operator code is isolated under one directory with zero references to customer-facing state, extracting it into a standalone package post-milestone is a `mv` command, not a rewrite — assuming the isolation rule in CONVENTIONS.md is actually followed during implementation.

### 2. Raspberry Pi removed — onboard x86 notebook instead

**Why:** more compute headroom, full ROS 2 desktop tooling, easier field debugging.

**What this means practically:**
- Every `Raspberry Pi` / `Pi` reference across all four docs is now `Onboard Compute (x86 Notebook)`.
- No code changes required in `edge_daemon/` — the existing `rclpy` stub-mode detection already handles "real ROS 2 present vs. not" as a runtime branch, and this resolves identically on x86.
- **One new consideration**: the notebook has its own battery, separate from the robot's traction battery. `robot_telemetry.battery_percent` is now explicitly scoped to the robot's traction battery only — don't let it get silently conflated with notebook battery state. If you need to track notebook battery too, that's a new, separate telemetry field — not a reuse of the existing one.
- MQTT topics, REST contracts, and all payload schemas are **unchanged**. This was a hardware substitution, not a protocol change.

---

## File Guide

### **ARCHITECTURE.md**
- New **"Known Technical Debt"** section documenting the operator-in-mobile decision, the four conditions attached to it, and the extraction plan.
- New **"Hardware topology"** section documenting the Pi → notebook pivot and the battery telemetry scoping consideration.
- Runtime diagram, node responsibility table, and network constraints all updated to reflect both pivots.

### **CONVENTIONS.md**
- Edge daemon section: stub-mode note (unaffected by hardware pivot), battery telemetry scoping rule.
- New **"Flutter — operator route (`mobile/lib/operator/`)"** section: directory isolation rules, hidden entry point guidance, staff auth token handling, tile cache / marker interpolation / WebSocket / form-based editing rules carried over unchanged from the original plan.
- Everything from the prior version regarding Go channel batching, PostGIS conventions, and the MQTT/REST write-path separation is **unchanged** — you were right to want to keep those.

### **PROTOCOL.md**
- New `POST /api/operator/auth/login` endpoint — required, not optional, for the hidden route's access control to mean anything.
- All `robot/*` MQTT topic tables updated: `Publisher`/`Subscriber` columns now say "Onboard notebook" instead of "Raspberry Pi (ROS 2)."
- Sealed result types split into "customer screens" and "operator route," both still living inside `mobile/lib/`.

### **STATE_FLOW.md**
- New **"Operator staff authentication — login flow"** sequence diagram — this is the actual security boundary, diagrammed explicitly so it can't be skipped by accident.
- All onboard-compute-participant diagrams relabeled from "Raspberry Pi (ROS 2)" to "Onboard notebook."
- New `staff_users` table schema, required to back the login flow.
- New schema note on `robot_telemetry.battery_percent` scoping.

---

## Backlog de acabamento (baixa prioridade, apos a integracao funcional)

- [ ] Gerar o APK com o nome de artefato `MARMITRON_3000` e icone oficial do projeto.
- [ ] Reformular a arte do display embarcado e expor logs de navegacao legiveis para apoio da equipe tecnica.
- [ ] Revisar icone do app, modo escuro e consistencia visual geral do Flutter.

Esses itens nao bloqueiam a demonstracao funcional. Devem ser tratados depois da telemetria, navegacao e video reais ou quando as dependencias externas estiverem aguardando retorno.

## Prioridade alta: integracao fisica App - ESP32

- [ ] Definir e documentar qual ESP32 fisico hospedara o firmware em `hardware/esp32-lock` (display, QR e atuador de retirada).
- [ ] Validar GPIOs, alimentacao, aterramento comum e isolamento do atuador antes de gravar o firmware.
- [ ] Provisionar credenciais MQTT exclusivas para o modulo escolhido e validar `display_qr`, `unlock` e heartbeat contra o gateway.

Nao reutilizar a ESP de sensores sem uma auditoria de pinos: ela ja usa SPI para IMU, UART para GPS, GPIOs para sonar/encoders e e sensivel a interferencia de display/atuador. O modulo de trava/display deve permanecer isolado da malha de motores sempre que houver uma terceira ESP disponivel.

## 2-Week Milestone Roadmap

Given the compressed timeline, here's the priority order. Items marked **[BLOCKING]** are required for the security model to hold — do not defer them to "after the demo."

### Days 1–3: Backend foundation
- [ ] Install PostGIS; run migrations for `robot_telemetry`, `deliveries`, `campus_restrictions`, **`staff_users`**
- [ ] **[BLOCKING]** Implement `POST /api/operator/auth/login` + staff auth middleware for all `/api/operator/*` routes
- [ ] Implement `TelemetryIngestService` with channel + batching worker (CONVENTIONS.md pattern) — this is unchanged from the original plan, keep it as-is
- [ ] Add telemetry MQTT subscriber to gateway startup

### Days 3–5: Edge daemon on the notebook
- [ ] Confirm ROS 2 desktop distro + `rclpy` resolve correctly on the onboard x86 notebook (should require zero code changes)
- [ ] Add 1 Hz `robot/telemetry` publish, scoped `battery_percent` to traction battery only
- [ ] Confirm heartbeat still carries `RobotState` values at QoS 1 for delivery status authority

### Days 5–8: Operator route inside `mobile/`
- [ ] Scaffold `mobile/lib/operator/` — screens, `OperatorApiService`, isolated state notifiers
- [ ] Implement hidden entry point (long-press or deep link) + staff login screen
- [ ] Implement `flutter_map` view pointed at the self-hosted tile cache — **do not point at public OSM tiles even temporarily**, the rate-limit risk is real on demo day
- [ ] Implement live delivery list + marker rendering with `AnimatedMapController` interpolation

### Days 8–11: Gateway operator endpoints
- [ ] `GET /api/operator/deliveries`
- [x] `GET /api/operator/deliveries/{id}/telemetry` (trilha persistida; lista de entregas ainda depende do contrato de destino)
- [ ] `GET /api/operator/campus/restrictions`
- [ ] `POST /api/operator/campus/restrictions` (form-based zone creation, not map-drawn)
- [ ] `GET /api/operator/ws/telemetry` WebSocket, with reconnect + resnapshot logic

### Days 11–13: Integration + tile cache
- [ ] Stand up self-hosted tile cache on EC2, seed for UnB campus bounding box
- [ ] End-to-end test: dispatch → telemetry flows → operator route shows live marker moving smoothly
- [ ] Test campus restriction refresh: create a zone → confirm nav2 (on the notebook) picks it up within 5 minutes, falls back to cache on network loss

### Days 13–14: Demo prep
- [ ] Confirm the hidden route entry point works reliably on the demo device
- [ ] Confirm staff login token doesn't expire mid-demo (set a generous `expires_at`)
- [ ] Rehearse the failure paths you can show live: MQTT drop → OTP-only badge; restriction cache fallback; telemetry replay for a completed delivery

---

## Non-Negotiable Checklist Before Demo Day

These are the items where cutting corners under time pressure creates a real problem, not just technical debt:

- [ ] `/api/operator/*` endpoints reject requests without a valid Bearer token (test with `curl`, no token, confirm 401)
- [ ] `campus_restrictions` has no write path from the MQTT subscriber (grep the codebase for any `INSERT INTO campus_restrictions` outside `operator_restrictions.go`)
- [ ] The Paho `OnMessage` callback for `robot/telemetry` never calls a DB method directly (grep for `s.db.` or `s.repo.` inside any `on*Message` function)
- [ ] Tile requests from the operator route go to the self-hosted cache, not `tile.openstreetmap.org`
- [ ] `edge_daemon/` has no `asyncpg`, `psycopg`, or `DATABASE_URL` reference anywhere

---

## Files Included

- `ARCHITECTURE.md` — System topology, hardware pivot, operator-in-mobile technical debt record
- `CONVENTIONS.md` — Enforcement layer: code style, directory isolation rules, invariants
- `PROTOCOL.md` — API contracts (REST + MQTT + sealed types), including new staff auth endpoint
- `STATE_FLOW.md` — State machines, diagrams, PostgreSQL schema including `staff_users`

Replace your existing `/docs` directory with these four files.
