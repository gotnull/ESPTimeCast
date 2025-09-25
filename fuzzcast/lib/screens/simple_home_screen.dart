import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:fuzzcast/api/api_service.dart';
import 'package:fuzzcast/screens/simple_config_screen.dart';
import 'package:http/http.dart' as http;

import 'dart:io';

class SimpleHomeScreen extends StatefulWidget {
  const SimpleHomeScreen({super.key});

  @override
  State<SimpleHomeScreen> createState() => _SimpleHomeScreenState();
}

class _SimpleHomeScreenState extends State<SimpleHomeScreen> {
  String? deviceIp;
  bool isLoading = false;
  final ApiService _api = ApiService();

  @override
  void initState() {
    super.initState();
    _loadDeviceIp();
  }

  _loadDeviceIp() async {
    final prefs = await SharedPreferences.getInstance();
    String? savedIp = prefs.getString('deviceIp');

    // Update old default IP to new one
    if (savedIp == '192.168.4.1') {
      savedIp = '192.168.4.72';
      await prefs.setString('deviceIp', savedIp);
    }

    setState(() {
      deviceIp = savedIp;
    });
  }

  _findDevice() async {
    setState(() {
      isLoading = true;
    });

    try {
      String? foundIp = await _smartDeviceDiscovery();

      if (foundIp != null) {
        setState(() {
          deviceIp = foundIp;
          isLoading = false;
        });

        final prefs = await SharedPreferences.getInstance();
        await prefs.setString('deviceIp', foundIp);

        if (mounted) {
          final scaffoldMessenger = ScaffoldMessenger.of(context);
          scaffoldMessenger.showSnackBar(
            const SnackBar(
              content: Text('Device connected!'),
              backgroundColor: Colors.green,
            ),
          );
        }
      } else {
        setState(() {
          isLoading = false;
        });

        if (mounted) {
          showDialog(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Setup Instructions'),
              content: const Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'To connect your FuzzCast device:',
                    style: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  SizedBox(height: 12),
                  Text('1. Make sure device is powered on'),
                  Text('2. Device should show scrolling text on display'),
                  Text('3. Connect device to your WiFi network'),
                  Text('4. Make sure your phone is on the same WiFi'),
                  Text('5. Try "Find Device" again'),
                  SizedBox(height: 12),
                  Text(
                    'If device shows "SETUP NEEDED", use the device setup in your ESP settings.',
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context),
                  child: const Text('Cancel'),
                ),

                ElevatedButton(
                  onPressed: () {
                    Navigator.pop(context);
                    _findDevice(); // Try again
                  },
                  child: const Text('Try Again'),
                ),
              ],
            ),
          );
        }
      }
    } catch (e) {
      setState(() {
        isLoading = false;
      });
      if (mounted) {
        final scaffoldMessenger = ScaffoldMessenger.of(context);
        scaffoldMessenger.showSnackBar(
          SnackBar(content: Text('Discovery failed: $e')),
        );
      }
    }
  }

  Future<String?> _smartDeviceDiscovery() async {
    // Get the device's local IP to determine network range
    String? networkBase = await _getNetworkBase();
    if (networkBase == null) return null;

    // Fast parallel scanning of likely IPs
    final List<Future<String?>> futures = [];

    // Scan the most likely range first (current network + common device IPs)
    for (int i = 100; i <= 120; i++) {
      futures.add(_testIpQuick('$networkBase.$i'));
    }

    // Also try some very common ESP device IPs
    final commonIps = [
      '192.168.4.72', // ESP AP mode default - try this first
      '$networkBase.1',
      '$networkBase.2',
      '$networkBase.10',
      '$networkBase.50',
    ];

    for (final ip in commonIps) {
      futures.add(_testIpQuick(ip));
    }

    // Wait for first successful connection
    try {
      final results = await Future.wait(futures);
      return results.firstWhere((ip) => ip != null, orElse: () => null);
    } catch (e) {
      return null;
    }
  }

  Future<String?> _getNetworkBase() async {
    try {
      // Get device's IP to figure out network range
      for (var interface in await NetworkInterface.list()) {
        for (var addr in interface.addresses) {
          if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
            final parts = addr.address.split('.');
            if (parts.length == 4) {
              return '${parts[0]}.${parts[1]}.${parts[2]}';
            }
          }
        }
      }
    } catch (e) {
      // Fallback to common ranges
    }
    return '192.168.1'; // Most common default
  }

  Future<String?> _testIpQuick(String ip) async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('deviceIp', ip);

      // Test the standard port 80 first since it works in browser
      try {
        final testUrl = 'http://$ip/config.json';
        debugPrint('Testing: $testUrl');

        final response = await http.get(
          Uri.parse(testUrl),
          headers: {
            'User-Agent': 'FuzzCast-App',
            'Accept': '*/*',
            'Connection': 'keep-alive',
          },
        ).timeout(const Duration(seconds: 5));

        if (response.statusCode == 200) {
          debugPrint('Success connecting to $ip');
          return ip;
        }
      } catch (e) {
        debugPrint('Connection failed for $ip: $e');
        // If it's a permission error, throw it up
        if (e.toString().toLowerCase().contains('operation not permitted')) {
          throw Exception('Network permission denied - check app permissions');
        }
      }
      return null;
    } catch (e) {
      debugPrint('IP test failed for $ip: $e');
      return null;
    }
  }

  _showIpDialog() {
    final controller = TextEditingController(text: deviceIp ?? '');

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Enter Device IP'),
        content: TextField(
          controller: controller,
          decoration: const InputDecoration(hintText: '192.168.1.100'),
          keyboardType: TextInputType.numberWithOptions(decimal: true),
          contextMenuBuilder: (context, editableTextState) =>
              const SizedBox.shrink(),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () async {
              final ip = controller.text.trim();
              if (ip.isNotEmpty) {
                Navigator.pop(context);
                await _setDeviceIp(ip);
              }
            },
            child: const Text('Connect'),
          ),
        ],
      ),
    );
  }

  _setDeviceIp(String ip) async {
    setState(() {
      isLoading = true;
    });

    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('deviceIp', ip);

      // Test connection
      await _api.getConfig();

      setState(() {
        deviceIp = ip;
        isLoading = false;
      });

      if (mounted) {
        final scaffoldMessenger = ScaffoldMessenger.of(context);
        scaffoldMessenger.showSnackBar(
          const SnackBar(
            content: Text('Connected!'),
            backgroundColor: Colors.green,
          ),
        );
      }
    } catch (e) {
      setState(() {
        isLoading = false;
      });
      if (mounted) {
        final scaffoldMessenger = ScaffoldMessenger.of(context);
        scaffoldMessenger.showSnackBar(
          SnackBar(content: Text('Connection failed: $e')),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('FuzzCast'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: deviceIp != null
                ? () {
                    setState(() {});
                    ScaffoldMessenger.of(
                      context,
                    ).showSnackBar(const SnackBar(content: Text('Refreshed')));
                  }
                : null,
          ),
        ],
      ),
      body: isLoading
          ? const Center(child: CircularProgressIndicator())
          : deviceIp == null
          ? Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.router, size: 80, color: Colors.grey),
                  const SizedBox(height: 24),
                  const Text(
                    'No Device Connected',
                    style: TextStyle(fontSize: 24),
                  ),
                  const SizedBox(height: 16),
                  ElevatedButton.icon(
                    onPressed: () {
                      setState(() {
                        isLoading = true;
                      });
                      _findDevice();
                    },
                    icon: isLoading
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(
                              strokeWidth: 2,
                              color: Colors.white,
                            ),
                          )
                        : const Icon(Icons.search),
                    label: Text(isLoading ? 'Searching...' : 'Find Device'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.blue,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(
                        horizontal: 32,
                        vertical: 16,
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextButton(
                    onPressed: _showIpDialog,
                    child: const Text('Enter IP manually (advanced)'),
                  ),
                ],
              ),
            )
          : Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Card(
                    color: Colors.green.shade50,
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        children: [
                          const Icon(
                            Icons.check_circle,
                            color: Colors.green,
                            size: 48,
                          ),
                          const SizedBox(height: 8),
                          const Text(
                            'FuzzCast Device Ready!',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                              color: Colors.green,
                            ),
                          ),
                          const SizedBox(height: 4),
                          Text('Connected to $deviceIp'),
                          const SizedBox(height: 8),
                          const Text(
                            'Your device is displaying time and weather.',
                            style: TextStyle(color: Colors.grey),
                            textAlign: TextAlign.center,
                          ),
                        ],
                      ),
                    ),
                  ),

                  const SizedBox(height: 16),

                  Row(
                    children: [
                      const SizedBox(width: 8),
                      Expanded(
                        child: ElevatedButton.icon(
                          onPressed: () {
                            Navigator.of(context).push(
                              MaterialPageRoute(
                                builder: (context) =>
                                    SimpleConfigScreen(deviceIp: deviceIp!),
                              ),
                            );
                          },
                          icon: const Icon(Icons.settings),
                          label: const Text('Config'),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.green,
                            foregroundColor: Colors.white,
                          ),
                        ),
                      ),
                    ],
                  ),

                  const SizedBox(height: 12),

                  TextButton.icon(
                    onPressed: () async {
                      final scaffoldMessenger = ScaffoldMessenger.of(context);

                      // Clear saved device IP and reset connection
                      final prefs = await SharedPreferences.getInstance();
                      await prefs.remove('deviceIp');

                      if (mounted) {
                        setState(() {
                          deviceIp = null;
                        });

                        scaffoldMessenger.showSnackBar(
                          const SnackBar(
                            content: Text(
                              'Connection reset! Use "Find Device" to reconnect.',
                            ),
                            backgroundColor: Colors.orange,
                          ),
                        );
                      }
                    },
                    icon: const Icon(Icons.refresh),
                    label: const Text('Reset Connection'),
                    style: TextButton.styleFrom(foregroundColor: Colors.red),
                  ),
                ],
              ),
            ),
    );
  }
}
