import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/user.dart';
import 'api_service.dart';

class UserService {
  const UserService();

  /// Get user by ID
  Future<User> getUserById(String userId) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/users/$userId');

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('User not found');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to get user: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return User.fromJson(data);
  }

  /// Create a new user
  Future<User> createUser({
    required String name,
    required String email,
    required String role,
    String? address,
  }) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/users');

    final body = jsonEncode({
      'name': name,
      'email': email,
      'role': role,
      if (address != null) 'address': address,
    });

    final response = await http
        .post(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode != 201) {
      final error = _extractError(response.body);
      throw Exception('Failed to create user: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return User.fromJson(data);
  }

  /// Update user information
  Future<User> updateUser({
    required String userId,
    String? name,
    String? email,
    String? address,
  }) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/users/$userId');

    final body = jsonEncode({
      if (name != null) 'name': name,
      if (email != null) 'email': email,
      if (address != null) 'address': address,
    });

    final response = await http
        .patch(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('User not found');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to update user: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return User.fromJson(data);
  }

  /// List all users (admin only)
  Future<List<User>> listUsers({
    String? role,
    int limit = 50,
    int offset = 0,
  }) async {
    final queryParams = <String, String>{
      'limit': limit.toString(),
      'offset': offset.toString(),
      if (role != null) 'role': role,
    };

    final uri = Uri.parse('${ApiService.baseUrl}/api/users')
        .replace(queryParameters: queryParams);

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to list users: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    final usersJson =
        (data['users'] as List<dynamic>? ?? const []).cast<Map<String, dynamic>>();

    return usersJson.map((u) => User.fromJson(u)).toList();
  }

  // ─── Helper methods ──────────────────────────────────────────────────────────

  String _extractError(String body) {
    try {
      final data = jsonDecode(body) as Map<String, dynamic>;
      return data['error'] as String? ?? 'Unknown error';
    } catch (_) {
      return 'Unknown error';
    }
  }
}
