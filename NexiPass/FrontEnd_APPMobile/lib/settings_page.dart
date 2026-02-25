import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'api_controller.dart';
import 'login_page.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final TextEditingController _ipController = TextEditingController();
  List<String> _ipHistory = [];
  bool _isLoading = true;

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _ipHistory = prefs.getStringList('ip_history') ?? [];
      _isLoading = false;
    });
  }

  Future<void> _continueWithIp(String ip) async {
    if (!_isValidIpPort(ip)) {
      _showSnack('Formato inválido! Use: IP:PORTA (ex: 192.168.1.242:5000)');
      return;
    }

    final prefs = await SharedPreferences.getInstance();
    
    // Guarda IP
    await prefs.setString('server_ip', ip);
    api.updateBaseUrl(ip);

    // Adiciona ao histórico
    if (!_ipHistory.contains(ip)) {
      _ipHistory.insert(0, ip);
      if (_ipHistory.length > 10) _ipHistory.removeLast();
      await prefs.setStringList('ip_history', _ipHistory);
    }

    // Vai para login
    Navigator.pushReplacement(
      context,
      MaterialPageRoute(builder: (_) => const MyHomePage(title: 'NexiPass Login')),
    );
  }

  bool _isValidIpPort(String input) {
    final regex = RegExp(r'^(\d{1,3}\.){3}\d{1,3}:\d+$');
    return regex.hasMatch(input);
  }

  void _showSnack(String message) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator()),
      );
    }

    return Scaffold(
      backgroundColor: Colors.green[100],
      appBar: AppBar(
        backgroundColor: Colors.green,
        title: const Text('Configuração do Servidor'),
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.dns, size: 80, color: Colors.green),
              const SizedBox(height: 20),
              const Text(
                'Configure o IP do servidor',
                style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.green),
              ),
              const SizedBox(height: 40),
              
              // Campo de input
              TextField(
                controller: _ipController,
                decoration: InputDecoration(
                  labelText: 'Server IP:PORT',
                  hintText: '192.168.1.242:5000',
                  prefixIcon: const Icon(Icons.computer),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                  filled: true,
                  fillColor: Colors.white,
                ),
              ),
              const SizedBox(height: 20),
              
              // Botão continuar
              ElevatedButton(
                onPressed: () => _continueWithIp(_ipController.text.trim()),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.green,
                  padding: const EdgeInsets.symmetric(horizontal: 50, vertical: 15),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                ),
                child: const Text('Continuar', style: TextStyle(fontSize: 18, color: Colors.white, fontWeight: FontWeight.bold)),
              ),
              
              // Histórico de IPs
              if (_ipHistory.isNotEmpty) ...[
                const SizedBox(height: 40),
                const Text('IPs anteriores:', style: TextStyle(fontSize: 16, color: Colors.grey)),
                const SizedBox(height: 10),
                ...(_ipHistory.take(3).map((ip) => Padding(
                  padding: const EdgeInsets.only(bottom: 8.0),
                  child: ElevatedButton(
                    onPressed: () => _continueWithIp(ip),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.white,
                      foregroundColor: Colors.green,
                      minimumSize: const Size(double.infinity, 50),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                    ),
                    child: Text(ip, style: const TextStyle(fontSize: 16)),
                  ),
                ))),
              ],
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _ipController.dispose();
    super.dispose();
  }
}