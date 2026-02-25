// lib/api_controller.dart
import 'dart:convert';
import 'package:http/http.dart' as http;


final api = ApiController(); // Instance of ApiController to be used throughout the app

class ApiController {
  String _baseUrl = '';  // Começa vazio - obriga configuração
  
  String? currentCardId;
  int? currentUserId;
  String? currentUserRole;
  
  // Remove o construtor antigo e a função _loadBaseUrl()
  // Não precisamos carregar automaticamente, o settings_page faz isso
  
  void updateBaseUrl(String ipPort) {
    _baseUrl = 'http://$ipPort';
  }
  
  // Resto das funções fica igual...

////////////////////////////////////////////////////////////////////////////////////////////////////////////////                                     
/////                                      FUNÇÕES DE DEBUG                                               /////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//***********************************************************************************************************/
//                                          LOGIN Debug                                                     //
//***********************************************************************************************************/

Future<Map<String, dynamic>> loginWithDebug(String username, String password) async {
  String debugInfo = '';
  
  try {
    debugInfo += '========== DEBUG LOGIN ==========\n';
    debugInfo += 'URL: $_baseUrl/login\n';
    debugInfo += 'Username: $username\n';
    debugInfo += 'A enviar pedido...\n\n';
    
    final resp = await http.post(
      Uri.parse('$_baseUrl/login'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'username': username,
        'password': password,
      }),
    );

    debugInfo += 'Status Code: ${resp.statusCode}\n';
    debugInfo += 'Response Body: ${resp.body}\n';

    if (resp.statusCode != 200) {
      return {'success': false, 'debug': debugInfo};
    }

    final data = jsonDecode(resp.body);
    bool success = data['ok'] == true;
    
    if (success && data.containsKey('user_id')) {
      currentUserId = data['user_id'];
      currentUserRole = data['role']; 
      debugInfo += '\nUser ID guardado: $currentUserId\n';
    }
    
    return {'success': success, 'debug': debugInfo};
    
  } catch (e) {
    debugInfo += '\nERRO: $e';
    return {'success': false, 'debug': debugInfo};
  }
}
//***********************************************************************************************************/
//***********************************************************************************************************/
//                                          ADD CONSUMPTION Debug                                           //
//***********************************************************************************************************/

Future<Map<String, dynamic>> addConsumptionWithDebug(int productId, int quantity) async {
  String debugInfo = '';
  
  if (currentCardId == null) {
    debugInfo = 'ERRO: currentCardId é null!';
    return {'success': false, 'debug': debugInfo};
  }
  
  if (currentUserId == null) {
    debugInfo = 'ERRO: currentUserId é null!';
    return {'success': false, 'debug': debugInfo};
  }
  
  try {
    debugInfo += '========== DEBUG ADD CONSUMPTION ==========\n';
    debugInfo += 'URL: $_baseUrl/add_consumption\n';
    debugInfo += 'Card ID: $currentCardId\n';
    debugInfo += 'Product ID: $productId\n';
    debugInfo += 'Quantity: $quantity\n';
    debugInfo += 'Employee ID: $currentUserId\n';
    debugInfo += 'A enviar pedido...\n\n';
    
    final resp = await http.post(
      Uri.parse('$_baseUrl/add_consumption'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'card_id': currentCardId,
        'product_id': productId,
        'quantity': quantity,
        'employee_id': currentUserId,
      }),
    );

    debugInfo += 'Status Code: ${resp.statusCode}\n';
    debugInfo += 'Response Body: ${resp.body}\n';

    if (resp.statusCode != 200) {
      return {'success': false, 'debug': debugInfo};
    }

    final data = jsonDecode(resp.body);
    bool success = data['ok'] == true;
    
    return {'success': success, 'debug': debugInfo};
    
  } catch (e) {
    debugInfo += '\nERRO: $e';
    return {'success': false, 'debug': debugInfo};
  }
}
//***********************************************************************************************************/
//***********************************************************************************************************/
//                                          Validate Exit Debug                                             //
//***********************************************************************************************************/
Future<Map<String, dynamic>> validateExitWithDebug() async {
  String debugInfo = '';
  
  if (currentCardId == null) {
    debugInfo = 'ERRO: currentCardId é null!';
    return {'success': false, 'debug': debugInfo};
  }
  
  try {
    debugInfo += '========== DEBUG VALIDATE EXIT ==========\n';
    debugInfo += 'URL: $_baseUrl/validate_exit\n';
    debugInfo += 'Card ID: $currentCardId\n';
    debugInfo += 'A enviar pedido...\n\n';
    
    final resp = await http.post(
      Uri.parse('$_baseUrl/validate_exit'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'card_id': currentCardId,
      }),
    );

    debugInfo += 'Status Code: ${resp.statusCode}\n';
    debugInfo += 'Response Body: ${resp.body}\n';

    if (resp.statusCode != 200) {
      return {'success': false, 'debug': debugInfo};
    }

    final data = jsonDecode(resp.body);
    bool success = data['closed'] == true;
    
    return {'success': success, 'debug': debugInfo};
    
  } catch (e) {
    debugInfo += '\nERRO: $e';
    return {'success': false, 'debug': debugInfo};
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                    Fim do Debug                                                      //////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////



  Future<String?> waitForCard() async {
    try {
      final resp = await http.get(
        Uri.parse('$_baseUrl/wait_card'),
      );

      if (resp.statusCode != 200) return null;

      final data = jsonDecode(resp.body);
      final id = data['card_id'] as String?;

      if (id != null) {
        currentCardId = id;
      }

      return id;
    } catch (_) {
      return null;
    }
  }

 
  Future<bool> activateCurrentCard(String phone) async {
    if (currentCardId == null) return false;

    try {
      final resp = await http.post(
        Uri.parse('$_baseUrl/activate_card'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'card_id': currentCardId,
          'phone': phone,
        }),
      );

      if (resp.statusCode != 200) return false;

      final data = jsonDecode(resp.body);
      return data['ok'] == true;
    } catch (_) {
      return false;
    }
  }



  
  Future<List<Product>> getProducts() async {
    try {
      final resp = await http.get(
        Uri.parse('$_baseUrl/products'),
      );

      if (resp.statusCode != 200) return [];

      final list = jsonDecode(resp.body) as List;
      return list
          .map((e) => Product.fromJson(e as Map<String, dynamic>))
          .toList();
    } catch (_) {
      return [];
    }
  }

 

 

  Future<CardSummary?> getConsumptionSummary() async {
    if (currentCardId == null) return null;

    try {
      final resp = await http.get(
        Uri.parse('$_baseUrl/card_summary?card_id=$currentCardId'),
      );

      if (resp.statusCode != 200) return null;

      final data = jsonDecode(resp.body) as Map<String, dynamic>;
      return CardSummary.fromJson(data);
    } catch (_) {
      return null;
    }
  }

  Future<bool> closeCard() async {
  if (currentCardId == null) return false;

  try {
    final resp = await http.post(
      Uri.parse('$_baseUrl/close_card'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'card_id': currentCardId,
      }),
    );

    if (resp.statusCode != 200) return false;

    final data = jsonDecode(resp.body);
    return data['ok'] == true;
  } catch (_) {
    return false;
  }
}

Future<List<ProductTotal>> getProductTotals() async {
  try {
    final resp = await http.get(
      Uri.parse('$_baseUrl/product_totals'),
    );

    if (resp.statusCode != 200) return [];

    final list = jsonDecode(resp.body) as List;
    return list
        .map((e) => ProductTotal.fromJson(e as Map<String, dynamic>))
        .toList();
  } catch (_) {
    return [];
  }
}
  
}

// ===========================================================================
//  MODELOS
// ===========================================================================

class Product {
  final int id;
  final String name;
  final double price;

  Product({
    required this.id,
    required this.name,
    required this.price,
  });

  factory Product.fromJson(Map<String, dynamic> j) {
    return Product(
      id: j['product_id'] as int,
      name: j['name'] as String,
      price: (j['price'] as num).toDouble(),
    );
  }
}

class ConsumptionLine {
  final String productName;
  final int quantity;
  final double unitPrice;
  final double total;

  ConsumptionLine({
    required this.productName,
    required this.quantity,
    required this.unitPrice,
    required this.total,
  });

  factory ConsumptionLine.fromJson(Map<String, dynamic> j) {
    return ConsumptionLine(
      productName: j['product_name'] as String,
      quantity: j['quantity'] as int,
      unitPrice: (j['unit_price'] as num).toDouble(),
      total: (j['total'] as num).toDouble(),
    );
  }
}

class CardSummary {
  final String cardId;
  final String phone;
  final double total;
  final List<ConsumptionLine> lines;

  CardSummary({
    required this.cardId,
    required this.phone,
    required this.total,
    required this.lines,
  });

  factory CardSummary.fromJson(Map<String, dynamic> j) {
    final linesJson = j['lines'] as List? ?? [];
    return CardSummary(
      cardId: j['card_id'] as String,
      phone: j['phone'] as String,
      total: (j['total'] as num).toDouble(),
      lines: linesJson
          .map((e) => ConsumptionLine.fromJson(e as Map<String, dynamic>))
          .toList(),
    );
  }
}


class ProductTotal {
  final String productName;
  final String employeeName;
  final int totalQuantity;
  final double totalRevenue;

  ProductTotal({
    required this.productName,
    required this.employeeName,
    required this.totalQuantity,
    required this.totalRevenue,
  });

  factory ProductTotal.fromJson(Map<String, dynamic> j) {
    return ProductTotal(
      productName: j['product_name'] as String,
      employeeName: j['employee_name'] as String,
      totalQuantity: j['total_quantity'] as int,
      totalRevenue: (j['total_revenue'] as num).toDouble(),
    );
  }
}