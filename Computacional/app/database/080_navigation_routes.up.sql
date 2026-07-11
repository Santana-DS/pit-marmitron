-- Navigation routes are versioned, approved paths owned by the robotics team.
-- The mobile app selects a destination; it never chooses intermediate nodes.
CREATE TABLE navigation_routes (
    route_id                text PRIMARY KEY,
    destination_point_key   text NOT NULL REFERENCES delivery_points(point_key) ON DELETE RESTRICT,
    label                   text NOT NULL,
    version                 integer NOT NULL DEFAULT 1 CHECK (version > 0),
    is_active               boolean NOT NULL DEFAULT true,
    execution_ready         boolean NOT NULL DEFAULT false,
    created_at              timestamptz NOT NULL DEFAULT now(),
    updated_at              timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX idx_navigation_routes_destination
    ON navigation_routes(destination_point_key, is_active);

CREATE TRIGGER navigation_routes_set_updated_at
BEFORE UPDATE ON navigation_routes
FOR EACH ROW EXECUTE FUNCTION set_updated_at();

-- Optional geographic route preview/audit. The edge executor may maintain its
-- own navigation representation, but approved node order remains reviewable.
CREATE TABLE navigation_route_nodes (
    route_id    text NOT NULL REFERENCES navigation_routes(route_id) ON DELETE CASCADE,
    sequence    integer NOT NULL CHECK (sequence >= 0),
    latitude    double precision NOT NULL CHECK (latitude BETWEEN -90 AND 90),
    longitude   double precision NOT NULL CHECK (longitude BETWEEN -180 AND 180),
    PRIMARY KEY (route_id, sequence)
);
