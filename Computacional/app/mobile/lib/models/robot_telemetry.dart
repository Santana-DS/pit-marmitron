// lib/models/robot_telemetry.dart
//
// Data model for the robot/telemetry MQTT payload, polled by the gateway
// via GET /api/robot/telemetry and consumed by the OperatorScreen.
//
// JSON shape mirrors the edge_daemon TelemetryCollector payload (telemetry.py).

/// The navigational / operational state of the robot.
enum RobotNavState {
  idle,
  navigating,
  arrived,
  offlineHold,
  fault,
  unknown;

  static RobotNavState fromString(String? value) {
    switch (value?.toUpperCase()) {
      case 'IDLE':
        return RobotNavState.idle;
      case 'NAVIGATING':
        return RobotNavState.navigating;
      case 'ARRIVED':
        return RobotNavState.arrived;
      case 'OFFLINE_HOLD':
        return RobotNavState.offlineHold;
      case 'FAULT':
        return RobotNavState.fault;
      default:
        return RobotNavState.unknown;
    }
  }

  String get label {
    switch (this) {
      case RobotNavState.idle:
        return 'Parado';
      case RobotNavState.navigating:
        return 'Em movimento';
      case RobotNavState.arrived:
        return 'Chegou ao destino';
      case RobotNavState.offlineHold:
        return 'Offline (retenção)';
      case RobotNavState.fault:
        return 'Falha';
      case RobotNavState.unknown:
        return 'Desconhecido';
    }
  }
}

class RobotPose {
  final double x;
  final double y;
  final double theta;
  final String frame;

  const RobotPose({
    required this.x,
    required this.y,
    required this.theta,
    required this.frame,
  });

  factory RobotPose.fromJson(Map<String, dynamic> json) => RobotPose(
        x: (json['x'] as num?)?.toDouble() ?? 0.0,
        y: (json['y'] as num?)?.toDouble() ?? 0.0,
        theta: (json['theta'] as num?)?.toDouble() ?? 0.0,
        frame: json['frame'] as String? ?? 'map',
      );
}

/// Full snapshot published by the edge daemon at ~2 Hz on robot/telemetry.
class RobotTelemetry {
  final RobotNavState navState;
  final String? activeOrderId;
  final RobotPose? pose;
  final double linearSpeedMps;
  final double batteryPercent;
  final double remainingMeters;
  final double progressPct;
  final double etaSeconds;
  final double cpuPct;
  final double memPct;
  final DateTime receivedAt;

  const RobotTelemetry({
    required this.navState,
    this.activeOrderId,
    this.pose,
    required this.linearSpeedMps,
    required this.batteryPercent,
    required this.remainingMeters,
    required this.progressPct,
    required this.etaSeconds,
    required this.cpuPct,
    required this.memPct,
    required this.receivedAt,
  });

  factory RobotTelemetry.fromJson(Map<String, dynamic> json) {
    final poseJson = json['pose'] as Map<String, dynamic>?;
    final velocityJson = json['velocity'] as Map<String, dynamic>?;
    final batteryJson = json['battery'] as Map<String, dynamic>?;
    return RobotTelemetry(
      navState: RobotNavState.fromString(json['nav_state'] as String?),
      activeOrderId: json['active_order_id'] as String?,
      pose: poseJson != null ? RobotPose.fromJson(poseJson) : null,
      linearSpeedMps:
          (velocityJson?['linear_mps'] as num?)?.toDouble() ?? 0.0,
      batteryPercent:
          (batteryJson?['percent'] as num?)?.toDouble() ?? 0.0,
      remainingMeters:
          (json['remaining_m'] as num?)?.toDouble() ?? 0.0,
      progressPct: (json['progress_pct'] as num?)?.toDouble() ?? 0.0,
      etaSeconds: (json['eta_seconds'] as num?)?.toDouble() ?? 0.0,
      cpuPct: (json['cpu_pct'] as num?)?.toDouble() ?? 0.0,
      memPct: (json['mem_pct'] as num?)?.toDouble() ?? 0.0,
      receivedAt: DateTime.now(),
    );
  }

  /// Converts speed from m/s to km/h for display.
  double get speedKmh => linearSpeedMps * 3.6;

  /// Returns ETA in minutes, rounded up.
  int get etaMinutes => (etaSeconds / 60).ceil();
}
