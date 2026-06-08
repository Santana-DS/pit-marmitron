/// User model representing a system user (client or restaurant)
/// This is the backend API model - for UI state, see UserModel in user_state.dart
class User {
  final String id;
  final String name;
  final String email;
  final String role;
  final String address;
  final DateTime createdAt;

  const User({
    required this.id,
    required this.name,
    required this.email,
    required this.role,
    required this.address,
    required this.createdAt,
  });

  bool get isClient => role == 'client';
  bool get isRestaurant => role == 'restaurant';

  factory User.fromJson(Map<String, dynamic> json) {
    return User(
      id: json['id'] as String,
      name: json['name'] as String,
      email: json['email'] as String,
      role: json['role'] as String,
      address: json['address'] as String? ?? '',
      createdAt: DateTime.parse(json['created_at'] as String),
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'email': email,
        'role': role,
        'address': address,
        'created_at': createdAt.toIso8601String(),
      };

  User copyWith({
    String? id,
    String? name,
    String? email,
    String? role,
    String? address,
    DateTime? createdAt,
  }) {
    return User(
      id: id ?? this.id,
      name: name ?? this.name,
      email: email ?? this.email,
      role: role ?? this.role,
      address: address ?? this.address,
      createdAt: createdAt ?? this.createdAt,
    );
  }
}
