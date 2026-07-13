-- DEMO ONLY. Never apply while a physical robot is subscribed to navigation.
-- The nodes use the Gazebo datum and exist solely to unblock the client flow.
INSERT INTO delivery_points (
    point_key, label, display_address, latitude, longitude,
    map_x, map_y, map_theta, map_frame, is_active, sort_order
) VALUES (
    'SIM_DEMO', 'Demonstracao simulada', 'Ambiente Gazebo',
    -15.000000, -47.000000, 0.0, 0.0, 0.0, 'map', true, 999
) ON CONFLICT (point_key) DO UPDATE SET is_active = true;

INSERT INTO navigation_routes (
    route_id, destination_point_key, label, version, is_active, execution_ready
) VALUES (
    'SIM_DEMO_V1', 'SIM_DEMO', 'Rota exclusiva de demonstracao', 1, true, true
) ON CONFLICT (route_id) DO UPDATE SET is_active = true, execution_ready = true;

INSERT INTO navigation_route_nodes (route_id, sequence, latitude, longitude) VALUES
    ('SIM_DEMO_V1', 0, -15.000000, -47.000000),
    ('SIM_DEMO_V1', 1, -15.000010, -47.000010)
ON CONFLICT (route_id, sequence) DO UPDATE SET latitude = EXCLUDED.latitude, longitude = EXCLUDED.longitude;
