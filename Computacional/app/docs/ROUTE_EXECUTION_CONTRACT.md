# Route Execution Contract

## Ownership

The Flutter app selects a delivery point. The gateway resolves an approved
`route_id`. The edge/ROS stack owns route execution, coordinate conversion,
obstacle handling and Nav2 goals. Neither Flutter nor the cloud gateway may
generate a path or send raw local poses as a substitute for an approved route.

## Command

The future route-aware edge executor must accept this payload on
`robot/commands/navigate`:

```json
{
  "order_id": "ABC123",
  "route_id": "ft_entrada_v1",
  "destination_point_key": "FT_ENTRADA",
  "issued_at": 1780000000
}
```

`route_id` is immutable. A route change creates a new ID/version rather than
mutating an active path.

## Required edge behaviour

1. Reject a missing, stale, inactive or unknown route without moving.
2. Resolve the route to the navigation representation required by the current
   ROS stack: GPS nodes, local map poses, or another validated planner input.
3. Publish an acknowledgement and lifecycle updates with `order_id` and
   `route_id`: `ACCEPTED`, `REJECTED`, `NAVIGATING`, `ARRIVED`, `FAILED`,
   `CANCELLED`.
4. Treat E-stop as higher priority than every route operation.
5. Never infer a route from only the final destination coordinates.

## Current integration status

The current edge daemon only accepts `pose.x/y/theta` and forwards one goal to
Nav2. It is **not route-aware yet**. Until Computacao delivers the resolver,
the gateway must keep route execution disabled and report the capability as
pending rather than publishing a command the daemon cannot execute safely.

## Data supplied by Computacao

For every approved route provide `route_id`, destination key, version, active
status and ordered geographic preview nodes. Also specify where the edge
obtains its executable route and how it persists/synchronizes revisions.
