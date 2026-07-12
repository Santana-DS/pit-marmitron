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

Computacao owns the physical-navigation decision. For every approved route,
they must survey/rehearse a safe path in the current simulation or robot map
and provide the following data to the app team:

```text
route_id: FT_ENTRADA_V1
destination_point_key: FT_ENTRADA
version: 1
nodes:
  - sequence: 0, latitude: -15.xxxxxx, longitude: -47.xxxxxx, theta: 0.0
  - sequence: 1, latitude: -15.xxxxxx, longitude: -47.xxxxxx, theta: 1.57
```

They must also confirm that `/fromLL` resolves against the same datum used by
their `navsat_transform_node`. The app team owns inserting the approved route
into `navigation_routes` and `navigation_route_nodes`, publishing it through
the gateway, and maintaining the app/edge contracts. Do not invent nodes from
the final destination, a map screenshot, or a latitude/longitude guess.
