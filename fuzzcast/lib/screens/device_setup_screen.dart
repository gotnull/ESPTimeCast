import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fuzzcast/api/api_service.dart';
import 'package:fuzzcast/models/config.dart';

class DeviceSetupScreen extends ConsumerStatefulWidget {
  const DeviceSetupScreen({super.key});

  @override
  ConsumerState<DeviceSetupScreen> createState() => _DeviceSetupScreenState();
}

class _DeviceSetupScreenState extends ConsumerState<DeviceSetupScreen> {
  final ApiService _apiService = ApiService();
  final TextEditingController _deviceIpController = TextEditingController();
  final TextEditingController _ssidController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();

  List<String> _discoveredDevices = [];
  List<WifiNetwork> _availableNetworks = [];
  WifiNetwork? _selectedNetwork;
  bool _isScanning = false;
  bool _isDiscovering = false;
  bool _isConfiguring = false;
  String? _statusMessage;

  @override
  void initState() {
    super.initState();
    _discoverDevices();
  }

  Future<void> _discoverDevices() async {
    setState(() {
      _isDiscovering = true;
      _statusMessage = 'Searching for ESP devices...';
    });

    try {
      final devices = await _apiService.discoverDevices();
      setState(() {
        _discoveredDevices = devices;
        _isDiscovering = false;
        _statusMessage = devices.isEmpty
          ? 'No devices found. Make sure device is powered on and connected to the same network.'
          : 'Found ${devices.length} device(s)';
      });
    } catch (e) {
      setState(() {
        _isDiscovering = false;
        _statusMessage = 'Error discovering devices: $e';
      });
    }
  }

  Future<void> _scanNetworks() async {
    if (_deviceIpController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please select or enter a device IP first')),
      );
      return;
    }

    setState(() {
      _isScanning = true;
      _statusMessage = 'Scanning for WiFi networks...';
    });

    try {
      final networks = await _apiService.scanNetworks(_deviceIpController.text);
      setState(() {
        _availableNetworks = networks;
        _isScanning = false;
        _statusMessage = 'Found ${networks.length} networks';
      });
    } catch (e) {
      setState(() {
        _isScanning = false;
        _statusMessage = 'Error scanning networks: $e';
      });
    }
  }

  Future<void> _configureWiFi() async {
    if (_ssidController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter WiFi SSID')),
      );
      return;
    }

    setState(() {
      _isConfiguring = true;
      _statusMessage = 'Configuring WiFi...';
    });

    try {
      // Update the device configuration with new WiFi credentials
      final config = Config(
        ssid: _ssidController.text,
        password: _passwordController.text,
      );

      await _apiService.updateConfig(config);

      // Save the device IP for future use
      await _apiService.setDeviceIp(_deviceIpController.text);

      setState(() {
        _isConfiguring = false;
        _statusMessage = 'WiFi configured successfully! Device will reboot.';
      });

      // Navigate back after a delay
      Future.delayed(const Duration(seconds: 3), () {
        if (mounted) Navigator.of(context).pop();
      });

    } catch (e) {
      setState(() {
        _isConfiguring = false;
        _statusMessage = 'Error configuring WiFi: $e';
      });
    }
  }

  

  Widget _buildSignalStrengthIcon(int strength) {
    return Icon(
      strength >= 4 ? Icons.signal_wifi_4_bar
        : strength >= 3 ? Icons.network_wifi_3_bar
        : strength >= 2 ? Icons.network_wifi_2_bar
        : Icons.network_wifi_1_bar,
      color: strength >= 3 ? Colors.green : strength >= 2 ? Colors.orange : Colors.red,
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Setup Device'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('Device Discovery', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
                    const SizedBox(height: 8),
                    if (_discoveredDevices.isNotEmpty) ...[
                      const Text('Found devices:'),
                      const SizedBox(height: 8),
                      ..._discoveredDevices.map((ip) => ListTile(
                        title: Text(ip),
                        trailing: _deviceIpController.text == ip
                          ? const Icon(Icons.check_circle, color: Colors.green)
                          : const Icon(Icons.radio_button_unchecked),
                        onTap: () {
                          setState(() {
                            _deviceIpController.text = ip;
                          });
                        },
                      )),
                    ],
                    const SizedBox(height: 8),
                    TextField(
                      controller: _deviceIpController,
                      decoration: const InputDecoration(
                        labelText: 'Device IP Address',
                        hintText: 'e.g., 192.168.1.100',
                      ),
                    ),
                    const SizedBox(height: 8),
                    ElevatedButton(
                      onPressed: _isDiscovering ? null : _discoverDevices,
                      child: _isDiscovering
                        ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                        : const Text('Discover Devices'),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('WiFi Configuration', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
                    const SizedBox(height: 8),

                    Row(
                      children: [
                        Expanded(
                          child: ElevatedButton(
                            onPressed: _isScanning ? null : _scanNetworks,
                            child: _isScanning
                              ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                              : const Text('Scan Networks'),
                          ),
                        ),
                      ],
                    ),

                    const SizedBox(height: 16),

                    if (_availableNetworks.isNotEmpty) ...[
                      const Text('Available Networks:'),
                      const SizedBox(height: 8),
                      SizedBox(
                        height: 150,
                        child: ListView.builder(
                          itemCount: _availableNetworks.length,
                          itemBuilder: (context, index) {
                            final network = _availableNetworks[index];
                            return ListTile(
                              leading: _buildSignalStrengthIcon(network.signalStrength),
                              title: Text(network.ssid),
                              subtitle: Text('${network.rssi} dBm'),
                              trailing: network.encrypted ? const Icon(Icons.lock) : const Icon(Icons.lock_open),
                              selected: _selectedNetwork == network,
                              onTap: () {
                                setState(() {
                                  _selectedNetwork = network;
                                  _ssidController.text = network.ssid;
                                });
                              },
                            );
                          },
                        ),
                      ),
                      const SizedBox(height: 16),
                    ],

                    TextField(
                      controller: _ssidController,
                      decoration: const InputDecoration(
                        labelText: 'WiFi SSID',
                        hintText: 'Enter network name',
                      ),
                    ),
                    const SizedBox(height: 8),
                    TextField(
                      controller: _passwordController,
                      decoration: const InputDecoration(
                        labelText: 'WiFi Password',
                        hintText: 'Enter network password',
                      ),
                      obscureText: true,
                    ),
                    const SizedBox(height: 16),
                    ElevatedButton(
                      onPressed: _isConfiguring ? null : _configureWiFi,
                      child: _isConfiguring
                        ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                        : const Text('Configure WiFi'),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            if (_statusMessage != null)
              Card(
                color: Theme.of(context).colorScheme.surfaceContainerHighest,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Text(
                    _statusMessage!,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _deviceIpController.dispose();
    _ssidController.dispose();
    _passwordController.dispose();
    super.dispose();
  }
}

