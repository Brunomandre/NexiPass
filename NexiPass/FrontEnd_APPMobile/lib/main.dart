import 'package:flutter/material.dart';
import 'settings_page.dart';  // ← Importa settings em vez de login

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'NexiPass',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.green),
      ),
      home: const SettingsPage(),  // ← Inicia com Settings em vez de Login
    );
  }
}