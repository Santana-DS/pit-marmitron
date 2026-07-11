-- Delivery points are the bridge between the geographic campus map shown in
-- Flutter and the local ROS 2 map frame consumed by Nav2.
--
-- Only points surveyed by the navigation team may be active. A client never
-- submits arbitrary ROS coordinates to the robot.
CREATE TABLE delivery_points (
    point_key       text PRIMARY KEY,
    label           text NOT NULL,
    display_address text NOT NULL,
    latitude        double precision NOT NULL CHECK (latitude BETWEEN -90 AND 90),
    longitude       double precision NOT NULL CHECK (longitude BETWEEN -180 AND 180),
    map_x           double precision NOT NULL,
    map_y           double precision NOT NULL,
    map_theta       double precision NOT NULL,
    map_frame       text NOT NULL DEFAULT 'map',
    is_active       boolean NOT NULL DEFAULT true,
    sort_order      integer NOT NULL DEFAULT 0,
    created_at      timestamptz NOT NULL DEFAULT now(),
    updated_at      timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX idx_delivery_points_active_sort
    ON delivery_points(is_active, sort_order, label);

CREATE TRIGGER delivery_points_set_updated_at
BEFORE UPDATE ON delivery_points
FOR EACH ROW EXECUTE FUNCTION set_updated_at();
