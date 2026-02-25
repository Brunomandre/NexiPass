import 'package:flutter/material.dart';
import 'api_controller.dart';
import 'card_actions_page.dart';
import 'point_of_sale_page.dart';

class EntryPage extends StatefulWidget {
  const EntryPage({super.key});

  @override
  State<EntryPage> createState() => _EntryPageState();
}

class _EntryPageState extends State<EntryPage> {
  String _statusText = 'Waiting for NFC connection';
  String? _cardId;
  bool _started = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (!_started) {
      _started = true;
      _waitForCard();
    }
  }

  Future<void> _waitForCard() async {
    setState(() {
      _statusText = 'Waiting for NFC connection';
      _cardId = null;
    });

    while (mounted) {
      final String? cardId = await api.waitForCard();

      if (!mounted) return;

      if (cardId != null) {
        setState(() {
          _cardId = cardId;
          _statusText = 'Cartão lido: $cardId';
        });
        return; 
      }

      
      await Future.delayed(const Duration(seconds: 1));
    }
  }

  void _navigateToNextPage() {
    final String? routeName = ModalRoute.of(context)?.settings.name;
    final bool isPOS = routeName == 'pos_entry';

    if (isPOS) {
      Navigator.push(
        context,
        MaterialPageRoute(
          builder: (_) => const PointOfSalePage(),
          settings: const RouteSettings(name: 'pos_menu'),
        ),
      );
    } else {
      Navigator.push(
        context,
        MaterialPageRoute(
          builder: (_) => const CardActionsPage(),
          settings: const RouteSettings(name: 'card_actions'),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final String? name = ModalRoute.of(context)?.settings.name;
    final bool posMode = name == 'pos_entry';

    return Scaffold(
      appBar: AppBar(
        title: Text(posMode ? 'Point of Sale - Waiting' : 'Entry'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.nfc, size: 72, color: Colors.green[700]),
              const SizedBox(height: 16),
              Text(
                _statusText,
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 18, color: Colors.black54),
              ),
              const SizedBox(height: 24),
              if (_cardId == null)
                const CircularProgressIndicator()
              else ...[
                ElevatedButton(
                  onPressed: _navigateToNextPage,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.green,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 36,
                      vertical: 12,
                    ),
                  ),
                  child: const Text('Continue'),
                ),
                const SizedBox(height: 16),
                ElevatedButton(
                  onPressed: _waitForCard,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 36,
                      vertical: 12,
                    ),
                  ),
                  child: const Text('New card'),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}