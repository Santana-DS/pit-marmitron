-- robot_telemetry stores best-effort observability data from the edge daemon.
-- order_id is orders.public_code, but intentionally has no foreign key:
-- mock/test order IDs must not poison a real telemetry batch insert.
--
-- pose_x/pose_y/pose_theta are ROS 2 frame coordinates in metres. map_frame
-- identifies whether the source was localized map pose (for example AMCL) or
-- odometry fallback. They are not GPS coordinates and must not be stored as
-- PostGIS SRID 4326 geometry without a calibrated transform.

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
    battery_percent    double precision,
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
