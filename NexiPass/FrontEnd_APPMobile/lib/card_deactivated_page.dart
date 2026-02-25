import 'package:flutter/material.dart';

class CardDeactivatedPage extends StatelessWidget {
  const CardDeactivatedPage({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Card deactivated'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.cancel, size: 72, color: Colors.red),
              const SizedBox(height: 16),
              const Text(
                'Card deactivated',
                style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 24),
              ElevatedButton(
                onPressed: () {
                  Navigator.of(context).popUntil(
                    (route) => route.settings.name == 'entry',
                  );
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.green,
                  padding: const EdgeInsets.symmetric(
                      horizontal: 36, vertical: 12),
                ),
                child: const Text('OK'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
