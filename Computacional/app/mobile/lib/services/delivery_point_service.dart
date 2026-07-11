import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/delivery_point.dart';
import 'api_service.dart';

class DeliveryPointService {
  const DeliveryPointService();

  Future<List<DeliveryPoint>> listActive() async {
    final response = await http
        .get(Uri.parse('${ApiService.baseUrl}/api/delivery-points'))
        .timeout(ApiService.apiTimeout);
    if (response.statusCode != 200) {
      throw Exception('Nao foi possivel carregar os pontos de entrega.');
    }

    final body = jsonDecode(response.body) as Map<String, dynamic>;
    final rawPoints = body['delivery_points'] as List<dynamic>? ?? const [];
    return rawPoints
        .whereType<Map<String, dynamic>>()
        .map(DeliveryPoint.fromJson)
        .toList();
  }
}
