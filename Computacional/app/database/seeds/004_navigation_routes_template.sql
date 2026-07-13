-- Fill only with a rehearsed route supplied by Computacao.
-- A changed path must use a new route_id/version; do not mutate a route in use.
INSERT INTO navigation_routes (
    route_id, destination_point_key, label, version, is_active, execution_ready
) VALUES (
    'FT_ENTRADA_V1', 'FT_ENTRADA', 'Rota segura para FT - Entrada', 1, true, true
);

INSERT INTO navigation_route_nodes (route_id, sequence, latitude, longitude) VALUES
    ('FT_ENTRADA_V1', 0, -15.000000, -47.000000),
    ('FT_ENTRADA_V1', 1, -15.000000, -47.000000);
