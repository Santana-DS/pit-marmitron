// lib/operator/screens/operator_screen.dart
//
// OPERATOR DASHBOARD
// ──────────────────────────────────────────────────────────────────────────────
// Exclusive to users with role 'operator'. Provides:
//
//   1. Real-time telemetry (polled every 3 s via GET /api/robot/telemetry)
//      • State badge (IDLE / NAVEGANDO / CHEGOU / OFFLINE_HOLD / FALHA)
//      • Battery percentage with colour-coded progress bar
//      • Speed (km/h), ETA (min), distance remaining (m), progress %
//      • CPU % and memory % for system health
//      • Pose (x, y, θ)
//      • Active order ID (if any)
//
//   2. Emergency Stop (POST /api/robot/estop)
//      • Large red button with double-confirmation dialog
//      • Clear warning when MQTT is unreachable (robot may not stop)
//      • Haptic heavy-impact on confirmation
//
//   3. Manual refresh button + auto-refresh indicator
//
// ARCHITECTURE
// ─────────────
// State is managed with a single _OperatorState object updated via setState().
// No provider / riverpod needed — the screen is ephemeral, never shared.
// Polling uses Timer.periodic; cancelled in dispose().
//
// ignore_for_file: prefer_const_constructors, prefer_const_literals_to_create_immutables

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:google_fonts/google_fonts.dart';

import '../../theme/app_theme.dart';
import '../../widgets/widgets.dart';
import '../models/robot_telemetry.dart';
import '../services/operator_service.dart';
import '../../main.dart';
import '../../screens/login_screen.dart';

// ─── Poll interval ─────────────────────────────────────────────────────────
const _kPollInterval = Duration(seconds: 3);

class OperatorScreen extends StatefulWidget {
  const OperatorScreen({super.key});

  @override
  State<OperatorScreen> createState() => _OperatorScreenState();
}

class _OperatorScreenState extends State<OperatorScreen>
    with SingleTickerProviderStateMixin {
  final _svc = OperatorService();

  RobotTelemetry? _telemetry;
  RobotCameraConfig? _cameraConfig;
  bool _loading = true;
  bool _estopInProgress = false;
  String? _errorMsg;
  String? _cameraError;
  DateTime? _lastUpdated;
  Timer? _pollTimer;

  // Pulse animation for the e-stop button
  late final AnimationController _pulseCtrl;
  late final Animation<double> _pulseAnim;

  @override
  void initState() {
    super.initState();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat(reverse: true);
    _pulseAnim = Tween<double>(begin: 1.0, end: 1.06).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );

    _fetchTelemetry();
    _fetchCameraConfig();
    _pollTimer = Timer.periodic(_kPollInterval, (_) => _fetchTelemetry());
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _pulseCtrl.dispose();
    super.dispose();
  }

  // ── Data fetching ─────────────────────────────────────────────────────────

  Future<void> _fetchTelemetry() async {
    final result = await _svc.fetchTelemetry();
    if (!mounted) return;
    setState(() {
      _loading = false;
      switch (result) {
        case TelemetryOk(:final data):
          _telemetry = data;
          _errorMsg = null;
          _lastUpdated = DateTime.now();
        case TelemetryNotAvailable():
          _errorMsg = null;
          _lastUpdated = DateTime.now();
        case TelemetryError(:final message):
          _errorMsg = message;
      }
    });
  }

  Future<void> _fetchCameraConfig() async {
    final result = await _svc.fetchCameraConfig();
    if (!mounted) return;
    setState(() {
      switch (result) {
        case CameraConfigOk(:final config):
          _cameraConfig = config;
          _cameraError = null;
        case CameraConfigError(:final message):
          _cameraError = message;
      }
    });
  }

  // ── Emergency Stop ────────────────────────────────────────────────────────

  Future<void> _onEstopPressed() async {
    HapticFeedback.mediumImpact();

    final confirmed = await showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => _EstopConfirmDialog(),
    );
    if (confirmed != true) return;

    HapticFeedback.heavyImpact();
    setState(() => _estopInProgress = true);

    final result = await _svc.sendEstop();
    if (!mounted) return;
    setState(() => _estopInProgress = false);

    switch (result) {
      case EstopSent():
        HapticFeedback.heavyImpact();
        _showBanner(
          message: '🛑 Parada de emergência enviada. Robô parando.',
          color: Colors.red.shade700,
          icon: Icons.stop_circle_rounded,
        );
      case EstopMqttUnreachable(:final message):
        HapticFeedback.heavyImpact();
        _showBanner(
          message: '⚠️ $message\nO robô pode não ter recebido o comando!',
          color: Colors.orange.shade800,
          icon: Icons.warning_rounded,
          duration: const Duration(seconds: 6),
        );
      case EstopNetworkError(:final message):
        _showBanner(
          message: message,
          color: Colors.grey.shade700,
          icon: Icons.cloud_off_rounded,
        );
    }
  }

  void _showBanner({
    required String message,
    required Color color,
    required IconData icon,
    Duration duration = const Duration(seconds: 4),
  }) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Row(children: [
          Icon(icon, color: Colors.white, size: 20),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              message,
              style: GoogleFonts.dmSans(
                  fontSize: 13, color: Colors.white, height: 1.4),
            ),
          ),
        ]),
        backgroundColor: color,
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        margin: const EdgeInsets.all(16),
        duration: duration,
      ),
    );
  }

  // ── Build ─────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AC.surface(context),
      body: CustomScrollView(
        slivers: [
          _buildAppBar(),
          SliverPadding(
            padding: const EdgeInsets.fromLTRB(20, 16, 20, 32),
            sliver: SliverList(
              delegate: SliverChildListDelegate([
                _buildStateCard(),
                const SizedBox(height: 14),
                _buildCameraCard(),
                const SizedBox(height: 14),
                _buildTelemetryGrid(),
                const SizedBox(height: 14),
                _buildSystemGrid(),
                const SizedBox(height: 14),
                if (_telemetry?.pose != null) ...[
                  _buildPoseCard(),
                  const SizedBox(height: 14),
                ],
                if (_telemetry?.activeOrderId != null) ...[
                  _buildOrderCard(),
                  const SizedBox(height: 14),
                ],
                _buildEstopButton(),
                const SizedBox(height: 8),
                _buildLastUpdated(),
              ]),
            ),
          ),
        ],
      ),
    );
  }

  // ── App Bar ───────────────────────────────────────────────────────────────

  Widget _buildAppBar() {
    return SliverAppBar(
      automaticallyImplyLeading: false,
      floating: true,
      backgroundColor: AC.surface(context),
      surfaceTintColor: Colors.transparent,
      title: Row(children: [
        Container(
          width: 36,
          height: 36,
          decoration: BoxDecoration(
            color: AppColors.primary,
            borderRadius: BorderRadius.circular(10),
          ),
          child: const Icon(Icons.settings_remote_rounded,
              color: Colors.white, size: 20),
        ),
        const SizedBox(width: 10),
        Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(
            'Painel do Operador',
            style: GoogleFonts.spaceGrotesk(
                fontSize: 15,
                fontWeight: FontWeight.w700,
                color: AC.primary(context)),
          ),
          Text(
            'Operador',
            style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context)),
          ),
        ]),
      ]),
      actions: [
        // Manual refresh
        if (_loading)
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: Center(
              child: SizedBox(
                width: 18,
                height: 18,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  color: AppColors.accent,
                ),
              ),
            ),
          )
        else
          IconButton(
            icon: Icon(Icons.refresh_rounded, color: AC.muted(context)),
            onPressed: () {
              hapticLight();
              setState(() => _loading = true);
              _fetchTelemetry();
            },
          ),
        // Settings / logout
        PopupMenuButton<String>(
          icon: Icon(Icons.more_vert_rounded, color: AC.muted(context)),
          color: AC.card(context),
          onSelected: (v) {
            if (v == 'logout') {
              Navigator.pushAndRemoveUntil(
                context,
                MaterialPageRoute(builder: (_) => const LoginScreen()),
                (r) => false,
              );
            } else if (v == 'theme') {
              themeModeNotifier.value =
                  themeModeNotifier.value == ThemeMode.dark
                      ? ThemeMode.light
                      : ThemeMode.dark;
            }
          },
          itemBuilder: (_) => [
            PopupMenuItem(
              value: 'theme',
              child: Text('Alternar tema',
                  style: GoogleFonts.dmSans(
                      fontSize: 13, color: AC.primary(context))),
            ),
            PopupMenuItem(
              value: 'logout',
              child: Text('Sair',
                  style: GoogleFonts.dmSans(
                      fontSize: 13, color: Colors.red.shade600)),
            ),
          ],
        ),
      ],
    );
  }

  // ── State Card ────────────────────────────────────────────────────────────

  Widget _buildStateCard() {
    final navState = _telemetry?.navState ?? RobotNavState.unknown;
    final cfg = _navStateConfig(navState);

    return AppCard(
      borderColor: cfg.color.withValues(alpha: 0.3),
      borderWidth: 1.5,
      child: Row(children: [
        Container(
          width: 48,
          height: 48,
          decoration: BoxDecoration(
            color: cfg.color.withValues(alpha: 0.12),
            borderRadius: BorderRadius.circular(12),
          ),
          child: Icon(cfg.icon, color: cfg.color, size: 24),
        ),
        const SizedBox(width: 14),
        Expanded(
          child:
              Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text(
              'Estado do Robô',
              style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context)),
            ),
            const SizedBox(height: 2),
            Row(children: [
              PulsingDot(color: cfg.color, size: 7),
              const SizedBox(width: 6),
              Text(
                navState.label,
                style: GoogleFonts.spaceGrotesk(
                    fontSize: 18,
                    fontWeight: FontWeight.w700,
                    color: cfg.color),
              ),
            ]),
          ]),
        ),
        if (_errorMsg != null)
          Icon(Icons.cloud_off_rounded, color: AC.muted(context), size: 20),
      ]),
    );
  }

  // ── Telemetry Grid ────────────────────────────────────────────────────────

  Widget _buildCameraCard() {
    final config = _cameraConfig;
    final kind = config?.streamKind.toLowerCase() ?? '';
    final canRenderInline = config != null &&
        config.available &&
        (kind == 'mjpeg' || kind == 'http' || kind == 'jpeg');

    return AppCard(
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(children: [
          Icon(Icons.videocam_rounded, color: AppColors.accent, size: 18),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              config?.label ?? 'Camera do robo',
              style: GoogleFonts.spaceGrotesk(
                  fontSize: 14,
                  fontWeight: FontWeight.w700,
                  color: AC.primary(context)),
            ),
          ),
          _CameraStatusChip(
            available: config?.available == true,
            error: _cameraError,
          ),
        ]),
        const SizedBox(height: 10),
        if (_cameraError != null)
          _CameraMessage(
            icon: Icons.cloud_off_rounded,
            text: _cameraError!,
            color: Colors.orange,
          )
        else if (config == null)
          _CameraMessage(
            icon: Icons.hourglass_empty_rounded,
            text: 'Carregando configuracao de video...',
            color: AC.muted(context),
          )
        else if (!config.available)
          _CameraMessage(
            icon: Icons.videocam_off_rounded,
            text:
                'Stream ainda nao configurado no gateway. Defina ROBOT_CAMERA_STREAM_URL.',
            color: AC.muted(context),
          )
        else if (canRenderInline)
          ClipRRect(
            borderRadius: BorderRadius.circular(10),
            child: AspectRatio(
              aspectRatio: 16 / 9,
              child: Image.network(
                config.streamUrl,
                fit: BoxFit.cover,
                gaplessPlayback: true,
                loadingBuilder: (context, child, progress) {
                  if (progress == null) return child;
                  return _CameraMessage(
                    icon: Icons.sync_rounded,
                    text: 'Conectando ao stream...',
                    color: AppColors.accent,
                  );
                },
                errorBuilder: (_, __, ___) => _CameraMessage(
                  icon: Icons.error_outline_rounded,
                  text: 'Nao foi possivel renderizar o stream HTTP/MJPEG.',
                  color: Colors.orange,
                ),
              ),
            ),
          )
        else
          _CameraMessage(
            icon: Icons.settings_input_antenna_rounded,
            text:
                'Stream ${config.streamKind} configurado. Viewer dedicado sera necessario.',
            color: AppColors.accent,
          ),
        if (config != null && config.available) ...[
          const SizedBox(height: 8),
          Text(
            '${config.streamKind} | alvo ${config.latencyTargetMs} ms',
            style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context)),
          ),
        ],
      ]),
    );
  }

  Widget _buildTelemetryGrid() {
    final t = _telemetry;
    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      SectionLabel('Telemetria'),
      Row(children: [
        Expanded(
          child: _MetricCard(
            icon: Icons.battery_charging_full_rounded,
            label: 'Bateria',
            value: t != null ? '${t.batteryPercent.toStringAsFixed(0)}%' : '--',
            sub: t != null ? _batteryBar(t.batteryPercent) : null,
            color: _batteryColor(t?.batteryPercent),
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: _MetricCard(
            icon: Icons.speed_rounded,
            label: 'Velocidade',
            value: t != null ? '${t.speedKmh.toStringAsFixed(1)} km/h' : '--',
            color: AppColors.accent,
          ),
        ),
      ]),
      const SizedBox(height: 10),
      Row(children: [
        Expanded(
          child: _MetricCard(
            icon: Icons.timer_outlined,
            label: 'ETA',
            value: t != null
                ? (t.etaSeconds > 0 ? '${t.etaMinutes} min' : '—')
                : '--',
            color: AppColors.purple,
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: _MetricCard(
            icon: Icons.straighten_rounded,
            label: 'Distância',
            value:
                t != null ? '${t.remainingMeters.toStringAsFixed(0)} m' : '--',
            color: AppColors.teal,
          ),
        ),
      ]),
      if (t != null && t.navState == RobotNavState.navigating) ...[
        const SizedBox(height: 10),
        _ProgressCard(progressPct: t.progressPct),
      ],
    ]);
  }

  // ── System Grid ───────────────────────────────────────────────────────────

  Widget _buildSystemGrid() {
    final t = _telemetry;
    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      SectionLabel('Sistema'),
      Row(children: [
        Expanded(
          child: _MetricCard(
            icon: Icons.developer_board_rounded,
            label: 'CPU',
            value: t != null ? '${t.cpuPct.toStringAsFixed(0)}%' : '--',
            sub: t != null ? _usageBar(t.cpuPct, Colors.orange) : null,
            color: Colors.orange,
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: _MetricCard(
            icon: Icons.memory_rounded,
            label: 'Memória',
            value: t != null ? '${t.memPct.toStringAsFixed(0)}%' : '--',
            sub: t != null ? _usageBar(t.memPct, AppColors.purple) : null,
            color: AppColors.purple,
          ),
        ),
      ]),
    ]);
  }

  // ── Pose Card ─────────────────────────────────────────────────────────────

  Widget _buildPoseCard() {
    final pose = _telemetry!.pose!;
    return AppCard(
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(children: [
          Icon(Icons.my_location_rounded, size: 16, color: AC.muted(context)),
          const SizedBox(width: 8),
          Text(
            'Posição (frame: ${pose.frame})',
            style: GoogleFonts.dmSans(fontSize: 12, color: AC.muted(context)),
          ),
        ]),
        const SizedBox(height: 12),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceAround,
          children: [
            _PoseValue(label: 'X', value: pose.x.toStringAsFixed(2)),
            _PoseValue(label: 'Y', value: pose.y.toStringAsFixed(2)),
            _PoseValue(label: 'θ (rad)', value: pose.theta.toStringAsFixed(3)),
          ],
        ),
      ]),
    );
  }

  // ── Active Order Card ─────────────────────────────────────────────────────

  Widget _buildOrderCard() {
    return AppCard(
      borderColor: AppColors.accent.withValues(alpha: 0.25),
      borderWidth: 1.5,
      child: Row(children: [
        Icon(Icons.receipt_long_rounded, color: AppColors.accent, size: 20),
        const SizedBox(width: 10),
        Expanded(
          child:
              Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text('Pedido em execução',
                style:
                    GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context))),
            const SizedBox(height: 2),
            Text(
              _telemetry!.activeOrderId!,
              style: GoogleFonts.spaceGrotesk(
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                  color: AC.primary(context)),
              overflow: TextOverflow.ellipsis,
            ),
          ]),
        ),
      ]),
    );
  }

  // ── E-Stop Button ─────────────────────────────────────────────────────────

  Widget _buildEstopButton() {
    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      SectionLabel('Controle de emergência'),
      ScaleTransition(
        scale: _pulseAnim,
        child: SizedBox(
          width: double.infinity,
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              onTap: _estopInProgress ? null : _onEstopPressed,
              borderRadius: BorderRadius.circular(16),
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                padding: const EdgeInsets.symmetric(vertical: 20),
                decoration: BoxDecoration(
                  color: _estopInProgress
                      ? Colors.red.shade900
                      : Colors.red.shade700,
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(
                    color: Colors.red.shade400,
                    width: 2,
                  ),
                ),
                child: _estopInProgress
                    ? const Center(
                        child: SizedBox(
                          width: 24,
                          height: 24,
                          child: CircularProgressIndicator(
                              strokeWidth: 2.5, color: Colors.white),
                        ),
                      )
                    : Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const Icon(Icons.stop_circle_rounded,
                              color: Colors.white, size: 26),
                          const SizedBox(width: 10),
                          Text(
                            'PARADA DE EMERGÊNCIA',
                            style: GoogleFonts.spaceGrotesk(
                              fontSize: 16,
                              fontWeight: FontWeight.w800,
                              color: Colors.white,
                              letterSpacing: 0.5,
                            ),
                          ),
                        ],
                      ),
              ),
            ),
          ),
        ),
      ),
      const SizedBox(height: 8),
      Text(
        'Publica robot/commands/estop via MQTT (QoS 2). '
        'O robô para imediatamente ao receber o comando.',
        style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context)),
        textAlign: TextAlign.center,
      ),
    ]);
  }

  // ── Last updated footer ────────────────────────────────────────────────────

  Widget _buildLastUpdated() {
    if (_errorMsg != null) {
      return Center(
        child: Text(
          '⚠️ $_errorMsg',
          style: GoogleFonts.dmSans(fontSize: 12, color: Colors.orange),
        ),
      );
    }
    if (_lastUpdated == null) return const SizedBox.shrink();
    final diff = DateTime.now().difference(_lastUpdated!).inSeconds;
    return Center(
      child: Text(
        'Atualizado há ${diff}s · auto-refresh a cada 3s',
        style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context)),
      ),
    );
  }

  // ── Helpers ───────────────────────────────────────────────────────────────

  ({Color color, IconData icon}) _navStateConfig(RobotNavState state) {
    switch (state) {
      case RobotNavState.idle:
        return (color: AppColors.teal, icon: Icons.pause_circle_rounded);
      case RobotNavState.navigating:
        return (color: AppColors.accent, icon: Icons.navigation_rounded);
      case RobotNavState.arrived:
        return (
          color: AppColors.purple,
          icon: Icons.check_circle_outline_rounded
        );
      case RobotNavState.offlineHold:
        return (color: Colors.orange, icon: Icons.wifi_off_rounded);
      case RobotNavState.fault:
        return (color: Colors.red, icon: Icons.error_outline_rounded);
      case RobotNavState.unknown:
        return (color: AC.muted(context), icon: Icons.help_outline_rounded);
    }
  }

  Color _batteryColor(double? pct) {
    if (pct == null) return AC.muted(context);
    if (pct > 60) return AppColors.teal;
    if (pct > 25) return Colors.orange;
    return Colors.red;
  }

  Widget _batteryBar(double pct) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(4),
      child: LinearProgressIndicator(
        value: pct / 100,
        backgroundColor: _batteryColor(pct).withValues(alpha: 0.15),
        color: _batteryColor(pct),
        minHeight: 4,
      ),
    );
  }

  Widget _usageBar(double pct, Color color) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(4),
      child: LinearProgressIndicator(
        value: pct / 100,
        backgroundColor: color.withValues(alpha: 0.15),
        color: color,
        minHeight: 4,
      ),
    );
  }
}

// ─── METRIC CARD ─────────────────────────────────────────────────────────────

class _CameraStatusChip extends StatelessWidget {
  final bool available;
  final String? error;

  const _CameraStatusChip({required this.available, this.error});

  @override
  Widget build(BuildContext context) {
    final color = error != null
        ? Colors.orange
        : available
            ? AppColors.teal
            : AC.muted(context);
    final label = error != null
        ? 'Erro'
        : available
            ? 'Ao vivo'
            : 'Pendente';

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: GoogleFonts.dmSans(
            fontSize: 10, fontWeight: FontWeight.w700, color: color),
      ),
    );
  }
}

class _CameraMessage extends StatelessWidget {
  final IconData icon;
  final String text;
  final Color color;

  const _CameraMessage({
    required this.icon,
    required this.text,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withValues(alpha: 0.18)),
      ),
      child: Row(children: [
        Icon(icon, color: color, size: 18),
        const SizedBox(width: 10),
        Expanded(
          child: Text(
            text,
            style: GoogleFonts.dmSans(
                fontSize: 12, color: AC.primary(context), height: 1.35),
          ),
        ),
      ]),
    );
  }
}

class _MetricCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final Color color;
  final Widget? sub;

  const _MetricCard({
    required this.icon,
    required this.label,
    required this.value,
    required this.color,
    this.sub,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AC.card(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AC.border(context)),
      ),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(children: [
          Icon(icon, size: 15, color: color),
          const SizedBox(width: 5),
          Text(label,
              style:
                  GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context))),
        ]),
        const SizedBox(height: 8),
        Text(value,
            style: GoogleFonts.spaceGrotesk(
                fontSize: 20, fontWeight: FontWeight.w700, color: color)),
        if (sub != null) ...[
          const SizedBox(height: 6),
          sub!,
        ],
      ]),
    );
  }
}

// ─── PROGRESS CARD ───────────────────────────────────────────────────────────

class _ProgressCard extends StatelessWidget {
  final double progressPct;
  const _ProgressCard({required this.progressPct});

  @override
  Widget build(BuildContext context) {
    return AppCard(
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text('Progresso da entrega',
                style:
                    GoogleFonts.dmSans(fontSize: 12, color: AC.muted(context))),
            Text('${progressPct.toStringAsFixed(0)}%',
                style: GoogleFonts.spaceGrotesk(
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                    color: AppColors.accent)),
          ],
        ),
        const SizedBox(height: 8),
        ClipRRect(
          borderRadius: BorderRadius.circular(6),
          child: LinearProgressIndicator(
            value: progressPct / 100,
            backgroundColor: AppColors.accent.withValues(alpha: 0.12),
            color: AppColors.accent,
            minHeight: 8,
          ),
        ),
      ]),
    );
  }
}

// ─── POSE VALUE ──────────────────────────────────────────────────────────────

class _PoseValue extends StatelessWidget {
  final String label;
  final String value;
  const _PoseValue({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Column(children: [
      Text(label,
          style: GoogleFonts.dmSans(fontSize: 11, color: AC.muted(context))),
      const SizedBox(height: 4),
      Text(value,
          style: GoogleFonts.spaceGrotesk(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: AC.primary(context))),
    ]);
  }
}

// ─── ESTOP CONFIRM DIALOG ─────────────────────────────────────────────────────

class _EstopConfirmDialog extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: AC.card(context),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
      icon:
          const Icon(Icons.warning_amber_rounded, color: Colors.red, size: 40),
      title: Text(
        'Confirmar parada de emergência?',
        style: GoogleFonts.spaceGrotesk(
            fontSize: 17,
            fontWeight: FontWeight.w700,
            color: AC.primary(context)),
        textAlign: TextAlign.center,
      ),
      content: Text(
        'O comando será publicado no tópico MQTT\n'
        'robot/commands/estop (QoS 2).\n\n'
        'O robô irá parar imediatamente. '
        'Certifique-se de que a área está segura.',
        style: GoogleFonts.dmSans(
            fontSize: 13, color: AC.muted(context), height: 1.5),
        textAlign: TextAlign.center,
      ),
      actionsAlignment: MainAxisAlignment.spaceEvenly,
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, false),
          child: Text('Cancelar',
              style:
                  GoogleFonts.dmSans(fontSize: 14, color: AC.muted(context))),
        ),
        FilledButton(
          style: FilledButton.styleFrom(
            backgroundColor: Colors.red.shade700,
            shape:
                RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
          onPressed: () {
            HapticFeedback.heavyImpact();
            Navigator.pop(context, true);
          },
          child: Text('PARAR ROBÔ',
              style: GoogleFonts.dmSans(
                  fontSize: 14,
                  fontWeight: FontWeight.w700,
                  color: Colors.white)),
        ),
      ],
    );
  }
}
