import 'dart:convert';
import 'dart:async';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:fuzzcast/models/config.dart';

class ApiService {
  static const String _deviceIpKey = 'deviceIp';

  Future<String?> getDeviceIp() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_deviceIpKey);
  }

  Future<void> setDeviceIp(String ip) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_deviceIpKey, ip);
  }

  Future<Config> getConfig() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }

    try {
      final response = await http.get(
        Uri.parse('http://$ip/config.json'),
        headers: {'Content-Type': 'application/json'},
      ).timeout(const Duration(seconds: 10));

      if (response.statusCode == 200) {
        return Config.fromJson(json.decode(response.body));
      } else {
        throw Exception('HTTP ${response.statusCode}: ${response.reasonPhrase}');
      }
    } catch (e) {
      throw Exception('Failed to connect to device at $ip: $e');
    }
  }

  Future<void> updateConfig(Config config) async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }

    final Map<String, dynamic> configMap = config.toJson();
    final Map<String, String> body = {};

    configMap.forEach((key, value) {
      if (key == 'countdown' && value is Map) {
        // Handle nested countdown object
        value.forEach((countdownKey, countdownValue) {
          if (countdownKey == 'targetTimestamp' && countdownValue != null) {
            // Convert timestamp to date and time strings for the ESP32
            final dateTime = DateTime.fromMillisecondsSinceEpoch(countdownValue * 1000);
            body['countdownDate'] = "${'${dateTime.year}'.padLeft(4, '0')}-${'${dateTime.month}'.padLeft(2, '0')}-${'${dateTime.day}'.padLeft(2, '0')}";
            body['countdownTime'] = "${'${dateTime.hour}'.padLeft(2, '0')}:${'${dateTime.minute}'.padLeft(2, '0')}";
          } else {
            body['countdown${countdownKey[0].toUpperCase()}${countdownKey.substring(1)}'] = countdownValue.toString();
          }
        });
      } else if (value != null) {
        body[key] = value.toString();
      }
    });

    // Special handling for durations (convert seconds to milliseconds for ESP32)
    if (config.clockDuration != null) {
      body['clockDuration'] = (config.clockDuration! * 1000).toString();
    }
    if (config.weatherDuration != null) {
      body['weatherDuration'] = (config.weatherDuration! * 1000).toString();
    }

    // Special handling for dimming times
    if (config.dimStartHour != null && config.dimStartMinute != null) {
      body['dimStartTime'] = "${'${config.dimStartHour}'.padLeft(2, '0')}:${'${config.dimStartMinute}'.padLeft(2, '0')}";
    }
    if (config.dimEndHour != null && config.dimEndMinute != null) {
      body['dimEndTime'] = "${'${config.dimEndHour}'.padLeft(2, '0')}:${'${config.dimEndMinute}'.padLeft(2, '0')}";
    }

    final response = await http.post(
      Uri.parse('http://$ip/save'),
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body,
    );
    if (response.statusCode != 200) {
      throw Exception('Failed to update config');
    }
  }

  Future<void> updateWiFiCredentials(String ssid, String password) async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }

    try {
      final response = await http.post(
        Uri.parse('http://$ip/save'),
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: Uri(queryParameters: {
          'ssid': ssid,
          'password': password,
        }).query,
      ).timeout(const Duration(seconds: 3));

      if (response.statusCode != 200) {
        throw Exception('HTTP ${response.statusCode}: ${response.body}');
      }
      // Success - got response within timeout
    } catch (e) {
      final errorStr = e.toString().toLowerCase();

      // These are all EXPECTED when device switches networks - treat as SUCCESS
      if (e is TimeoutException ||
          errorStr.contains('timeout') ||
          errorStr.contains('connection refused') ||
          errorStr.contains('no route to host') ||
          errorStr.contains('network is unreachable') ||
          errorStr.contains('socket') ||
          errorStr.contains('connection closed')) {
        // Device is switching networks - this is normal and expected
        return; // SUCCESS
      } else {
        // Real error - rethrow
        throw Exception('WiFi update failed: $e');
      }
    }
  }

  Future<void> restoreBackup() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.post(Uri.parse('http://$ip/restore'));
    if (response.statusCode != 200) {
      throw Exception('Failed to restore backup');
    }
  }

  // Individual setting update methods (for live updates)
  Future<void> setBrightness(int value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_brightness'), body: {'value': value.toString()});
  }

  Future<void> setFlipDisplay(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_flip'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setTwelveHourToggle(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_twelvehour'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setShowDayOfWeek(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_dayofweek'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setShowDate(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_showdate'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setShowHumidity(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_humidity'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setColonBlinkEnabled(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_colon_blink'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setLanguage(String value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_language'), body: {'value': value});
  }

  Future<void> setShowWeatherDescription(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_weatherdesc'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setWeatherUnits(bool isImperial) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_units'), body: {'value': isImperial ? '1' : '0'});
  }

  Future<void> setCountdownEnabled(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_countdown_enabled'), body: {'value': value ? '1' : '0'});
  }

  Future<void> setIsDramaticCountdown(bool value) async {
    final ip = await getDeviceIp();
    if (ip == null) throw Exception('Device IP not set');
    await http.post(Uri.parse('http://$ip/set_dramatic_countdown'), body: {'value': value ? '1' : '0'});
  }

  Future<Map<String, dynamic>> getTime() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.get(Uri.parse('http://$ip/time'));
    if (response.statusCode == 200) {
      return json.decode(response.body);
    } else {
      throw Exception('Failed to get time');
    }
  }

  Future<void> forceNtpUpdate() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.get(Uri.parse('http://$ip/ntp'));
    if (response.statusCode != 200) {
      throw Exception('Failed to force NTP update');
    }
  }

  Future<void> rebootDevice() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.get(Uri.parse('http://$ip/reboot'));
    if (response.statusCode != 200) {
      throw Exception('Failed to reboot device');
    }
  }

  Future<Map<String, dynamic>> getForecast() async {
    final ip = await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.get(Uri.parse('http://$ip/forecast'));
    if (response.statusCode == 200) {
      return json.decode(response.body);
    } else {
      throw Exception('Failed to get forecast');
    }
  }

  // Network scanning and provisioning methods
  Future<List<WifiNetwork>> scanNetworks([String? deviceIp]) async {
    final ip = deviceIp ?? await getDeviceIp();
    if (ip == null) {
      throw Exception('Device IP not set');
    }
    final response = await http.get(Uri.parse('http://$ip/scan_networks'));
    if (response.statusCode == 200) {
      final List<dynamic> networks = json.decode(response.body);
      return networks.map((network) => WifiNetwork.fromJson(network)).toList();
    } else {
      throw Exception('Failed to scan networks');
    }
  }

  // Device discovery methods
  Future<List<String>> discoverDevices() async {
    final List<String> devices = [];

    // Try common IP ranges on current network
    // This is a simplified approach - in production you'd use proper mDNS discovery
    final List<Future<void>> futures = [];

    for (int i = 1; i <= 254; i++) {
      futures.add(_checkDevice('192.168.1.$i').then((isDevice) {
        if (isDevice) devices.add('192.168.1.$i');
      }).catchError((_) {}));
    }

    await Future.wait(futures);
    return devices;
  }

  Future<bool> _checkDevice(String ip) async {
    try {
      final response = await http.get(
        Uri.parse('http://$ip/config.json'),
      ).timeout(const Duration(seconds: 1));
      return response.statusCode == 200;
    } catch (e) {
      return false;
    }
  }
}

class WifiNetwork {
  final String ssid;
  final int rssi;
  final bool encrypted;

  WifiNetwork({required this.ssid, required this.rssi, required this.encrypted});

  factory WifiNetwork.fromJson(Map<String, dynamic> json) {
    return WifiNetwork(
      ssid: json['ssid'],
      rssi: json['rssi'],
      encrypted: json['encryption'] == 1,
    );
  }

  int get signalStrength {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    return 1;
  }
}