# Simulated Route Demo

This is a simulation-only demonstration. Do not insert its nodes in a production route.

## UI and display simulation

Without ROS or a physical robot, publish a safe telemetry/status demonstration
from PowerShell. It does not publish `robot/commands/navigate` and therefore
cannot start Nav2:

```powershell
cd Computacional/app
python -m edge_daemon.sim_command demo --duration-seconds 30 --destination SIM_DEMO
```

The command requires the normal `edge_daemon/.env` MQTT settings. It updates
the Flutter operator map through `robot/telemetry` and the ESP32 display through
`robot/nav/status`, then ends at `ARRIVED`.

1. Start the ROS/Gazebo stack and confirm `/fromLL` and `/navigate_to_pose` exist.
2. Start `python -m edge_daemon` with its MQTT `.env`.
3. Listen for lifecycle messages:
   `python -m edge_daemon.sim_command listen --seconds 90`
4. In another terminal, publish two safe nodes chosen by Computacao for the current Gazebo world:
   `python -m edge_daemon.sim_command route --node <lat>,<lon>,<theta> --node <lat>,<lon>,<theta>`
5. Demonstrate E-stop while a node is active:
   `python -m edge_daemon.sim_command estop --reason demo_estop`

The operator app must show the route as simulation pending until surveyed production data is inserted.
