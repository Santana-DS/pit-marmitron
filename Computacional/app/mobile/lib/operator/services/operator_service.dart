// lib/operator/services/operator_service.dart
//
// HTTP client for operator-only endpoints:
//   GET  /api/robot/telemetry   — latest telemetry snapshot from the gateway
//   POST /api/robot/estop       — publishes robot/commands/estop via MQTT
//
// Callers (OperatorScreen) use sealed result types so every outcome is
// handled exhaustively at the call site — no raw status codes leak through.

import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import '../../config/app_config.dart';
import '../models/robot_telemetry.dart';

const Duration _kTimeout = Duration(seconds: 8);

// ─── E-STOP RESULT ──────────────────────────────────────────────────────────

sealed class EstopResult {
  const EstopResult();
}

/// Gateway published robot/commands/estop — robot received the command.
final class EstopSent extends EstopResult {
  const EstopSent();
}

/// MQTT broker unreachable — robot may NOT have received the command.
final class EstopMqttUnreachable extends EstopResult {
  final String message;
  const EstopMqttUnreachable({
    this.message =
        'Broker MQTT inacessível. Robô pode não ter recebido o comando.',
  });
}

/// Network-layer failure (timeout, no connectivity, unexpected error).
final class EstopNetworkError extends EstopResult {
  final String message;
  const EstopNetworkError({
    this.message = 'Sem conexão com o servidor. Verifique sua internet.',
  });
}

// ─── TELEMETRY RESULT ────────────────────────────────────────────────────────

sealed class TelemetryResult {
  const TelemetryResult();
}

/// A fresh telemetry snapshot was returned by the gateway.
final class TelemetryOk extends TelemetryResult {
  final RobotTelemetry data;
  const TelemetryOk(this.data);
}

/// Gateway has no recent telemetry (robot never connected or is silent).
final class TelemetryNotAvailable extends TelemetryResult {
  const TelemetryNotAvailable();
}

/// Network or server error while fetching telemetry.
final class TelemetryError extends TelemetryResult {
  final String message;
  const TelemetryError(this.message);
}

// Camera stream config.
sealed class CameraConfigResult {
  const CameraConfigResult();
}

final class CameraConfigOk extends CameraConfigResult {
  final RobotCameraConfig config;
  const CameraConfigOk(this.config);
}

final class CameraConfigError extends CameraConfigResult {
  final String message;
  const CameraConfigError(this.message);
}

class RobotCameraConfig {
  final bool available;
  final String streamUrl;
  final String streamKind;
  final String signalingUrl;
  final List<String> iceServers;
  final String label;
  final int latencyTargetMs;
  final String rosImageTopic;
  final String rosCompressedTopic;

  const RobotCameraConfig({
    required this.available,
    required this.streamUrl,
    required this.streamKind,
    required this.signalingUrl,
    required this.iceServers,
    required this.label,
    required this.latencyTargetMs,
    required this.rosImageTopic,
    required this.rosCompressedTopic,
  });

  factory RobotCameraConfig.fromJson(Map<String, dynamic> json) {
    return RobotCameraConfig(
      available: json['available'] as bool? ?? false,
      streamUrl: json['stream_url'] as String? ?? '',
      streamKind: json['stream_kind'] as String? ?? 'unset',
      signalingUrl: json['signaling_url'] as String? ?? '',
      iceServers: (json['ice_servers'] as List<dynamic>? ?? const [])
          .whereType<String>()
          .toList(),
      label: json['label'] as String? ?? 'C920 front camera',
      latencyTargetMs: json['latency_target_ms'] as int? ?? 500,
      rosImageTopic: json['ros_image_topic'] as String? ?? '/camera/image_raw',
      rosCompressedTopic:
          json['ros_compressed_topic'] as String? ??
          '/camera/image_raw/compressed',
    );
  }
}

// ─── OPERATOR SERVICE ────────────────────────────────────────────────────────

class OperatorService {
  static String get _base => AppConfig.apiBaseUrl;

  // ── sendEstop ──────────────────────────────────────────────────────────────
  //
  // POSTs to /api/robot/estop.
  // The gateway publishes {"source":"operator","timestamp":...} to the
  // robot/commands/estop MQTT topic at QoS 2 (exactly-once delivery).
  //
  // On EstopMqttUnreachable the caller MUST warn the user that the robot
  // may still be moving — this is a safety-critical path.
  Future<EstopResult> sendEstop() async {
    final url = Uri.parse('$_base/api/robot/estop');
    try {
      final response = await http
          .post(
            url,
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode({
              'source': 'operator_app',
              'timestamp': DateTime.now().toUtc().millisecondsSinceEpoch,
            }),
          )
          .timeout(_kTimeout);

      if (response.statusCode == 200) {
        debugPrint('[OperatorService] estop sent successfully');
        return const EstopSent();
      }

      final body = _tryParseJson(response.body);
      final detail = body?['error'] as String? ?? 'Unknown error';
      debugPrint(
        '[OperatorService] estop failed — HTTP ${response.statusCode}: $detail',
      );

      if (response.statusCode == 502) {
        return EstopMqttUnreachable(message: detail);
      }
      return EstopNetworkError(message: detail);
    } on TimeoutException {
      debugPrint('[OperatorService] estop timed out');
      return const EstopNetworkError(
        message: 'Tempo de resposta excedido. Tente novamente.',
      );
    } on Exception catch (e) {
      debugPrint('[OperatorService] estop error: $e');
      return const EstopNetworkError();
    }
  }

  // ── fetchTelemetry ─────────────────────────────────────────────────────────
  //
  // GETs /api/robot/telemetry — the gateway returns the last snapshot it
  // received on robot/telemetry (MQTT). If no snapshot has arrived since
  // startup, the gateway returns 204 No Content → TelemetryNotAvailable.
  Future<TelemetryResult> fetchTelemetry() async {
    final url = Uri.parse('$_base/api/robot/telemetry');
    try {
      final response = await http.get(url).timeout(_kTimeout);

      if (response.statusCode == 200) {
        final json = _tryParseJson(response.body);
        if (json == null) {
          return const TelemetryError('Resposta inválida do servidor.');
        }
        return TelemetryOk(RobotTelemetry.fromJson(json));
      }
      if (response.statusCode == 204) return const TelemetryNotAvailable();

      final body = _tryParseJson(response.body);
      return TelemetryError(
        body?['error'] as String? ?? 'Erro ${response.statusCode}',
      );
    } on TimeoutException {
      return const TelemetryError('Tempo de resposta excedido.');
    } on Exception catch (e) {
      debugPrint('[OperatorService] fetchTelemetry error: $e');
      return const TelemetryError('Erro de conexão.');
    }
  }

  Future<CameraConfigResult> fetchCameraConfig() async {
    final url = Uri.parse('$_base/api/robot/camera');
    try {
      final response = await http.get(url).timeout(_kTimeout);
      if (response.statusCode == 200) {
        final json = _tryParseJson(response.body);
        if (json == null) {
          return const CameraConfigError('Resposta inválida do servidor.');
        }
        return CameraConfigOk(RobotCameraConfig.fromJson(json));
      }

      final body = _tryParseJson(response.body);
      return CameraConfigError(
        body?['error'] as String? ?? 'Erro ${response.statusCode}',
      );
    } on TimeoutException {
      return const CameraConfigError('Tempo de resposta excedido.');
    } on Exception catch (e) {
      debugPrint('[OperatorService] fetchCameraConfig error: $e');
      return const CameraConfigError('Erro de conexão.');
    }
  }

  static Map<String, dynamic>? _tryParseJson(String body) {
    try {
      return jsonDecode(body) as Map<String, dynamic>;
    } catch (_) {
      return null;
    }
  }
}
