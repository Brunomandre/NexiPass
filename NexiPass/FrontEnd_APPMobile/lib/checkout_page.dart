import 'package:flutter/material.dart';
import 'api_controller.dart';

class CheckoutPage extends StatefulWidget {
  const CheckoutPage({super.key});

  @override
  State<CheckoutPage> createState() => _CheckoutPageState();
}

class _CheckoutPageState extends State<CheckoutPage> {
  bool _isWaitingCard = true;
  bool _isLoading = false;
  String _statusText = 'Waiting for NFC connection';
  //String? _cardId;
  String? _error;
  CardSummary? _summary;
  bool _showCardRead = false; // Nova flag para mostrar cartão lido

  @override
  void initState() {
    super.initState();
    _waitForCard();
  }

  Future<void> _waitForCard() async {
    setState(() {
      _isWaitingCard = true;
      _statusText = 'Waiting for NFC connection';
      api.currentCardId = null;
      _error = null;
      _summary = null;
      _showCardRead = false;
    });

    final String? cardId = await api.waitForCard();

    if (!mounted) return;

    if (cardId == null) {
      setState(() {
        _statusText = 'Waiting for NFC connection';
      });
      await Future.delayed(const Duration(seconds: 1));
      if (mounted) _waitForCard();
      return;
    }

    // Cartão lido - mostrar página intermediária
    setState(() {
      _isWaitingCard = false;
      api.currentCardId = cardId;
      _statusText = 'Cartão lido: $cardId';
      _showCardRead = true;
    });
  }

  Future<void> _loadSummary() async {
    setState(() {
      _isLoading = true;
      _error = null;
      _summary = null;
      _showCardRead = false;
    });

    final CardSummary? s = await api.getConsumptionSummary();

    if (!mounted) return;

    if (s == null) {
      setState(() {
        _isLoading = false;
        _error = 'Não foi possível obter o total do cartão atual.';
      });
      return;
    }

    setState(() {
      _isLoading = false;
      _summary = s;
    });
  }

  Future<void> _confirmCheckout(BuildContext context) async {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );

    final bool ok = await api.closeCard();

    Navigator.of(context).pop();

    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Erro ao fechar conta.')),
      );
      return;
    }

    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Sucesso'),
        content: const Text('Checkout completed successfully.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('OK'),
          ),
        ],
      ),
    );

    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Checkout'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: _isWaitingCard
              ? Column(
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
                    const CircularProgressIndicator(),
                  ],
                )
              : _showCardRead
                  ? _buildCardReadPage()
                  : _isLoading
                      ? const CircularProgressIndicator()
                      : _error != null
                          ? _buildErrorPage()
                          : _buildSummaryPage(context),
        ),
      ),
    );
  }

  // Nova página mostrando cartão lido (igual ao Entry)
  Widget _buildCardReadPage() {
    return Column(
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
        ElevatedButton(
          onPressed: _loadSummary,
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
    );
  }

  Widget _buildErrorPage() {
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Text(
          _error!,
          textAlign: TextAlign.center,
          style: const TextStyle(color: Colors.red),
        ),
        const SizedBox(height: 16),
        ElevatedButton(
          onPressed: _loadSummary,
          child: const Text('Retry'),
        ),
        const SizedBox(height: 16),
        ElevatedButton(
          onPressed: () => Navigator.pop(context),
          style: ElevatedButton.styleFrom(
            backgroundColor: Colors.green,
            padding: const EdgeInsets.symmetric(
              horizontal: 48,
              vertical: 14,
            ),
          ),
          child: const Text('Back'),
        ),
      ],
    );
  }

  Widget _buildSummaryPage(BuildContext context) {
    final s = _summary!;
    
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Card(
          elevation: 4,
          child: Padding(
            padding: const EdgeInsets.all(24.0),
            child: Column(
              children: [
                const Text(
                  'Total Amount:',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 12),
                Text(
                  '€ ${s.total.toStringAsFixed(2)}',
                  style: const TextStyle(
                    fontSize: 32,
                    fontWeight: FontWeight.bold,
                    color: Colors.green,
                  ),
                ),
                const SizedBox(height: 12),
                Text('Card ID: ${s.cardId}'),
                if (api.currentUserRole == "OWNER")
                  Text('Phone: ${s.phone}'),
              ],
            ),
          ),
        ),
        const SizedBox(height: 32),
        ElevatedButton(
          onPressed: () => _confirmCheckout(context),
          style: ElevatedButton.styleFrom(
            backgroundColor: Colors.green,
            padding: const EdgeInsets.symmetric(
              horizontal: 48,
              vertical: 14,
            ),
          ),
          child: const Text(
            'Close Account',
            style: TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
      ],
    );
  }
}