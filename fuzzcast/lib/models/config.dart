class Config {
  final String? ssid;
  final String? password;
  final String? openWeatherApiKey;
  final String? openWeatherCity;
  final String? openWeatherCountry;
  final String? weatherUnits;
  final String? timeZone;
  final String? language;
  final int? clockDuration;
  final int? weatherDuration;
  final int? brightness;
  final bool? flipDisplay;
  final bool? twelveHourToggle;
  final bool? showDayOfWeek;
  final bool? showDate;
  final bool? showHumidity;
  final bool? colonBlinkEnabled;
  final bool? showWeatherDescription;
  final String? ntpServer1;
  final String? ntpServer2;
  final bool? dimmingEnabled;
  final int? dimStartHour;
  final int? dimStartMinute;
  final int? dimEndHour;
  final int? dimEndMinute;
  final int? dimBrightness;
  final bool? countdownEnabled;
  final int? countdownTargetTimestamp;
  final String? countdownLabel;
  final bool? isDramaticCountdown;

  Config({
    this.ssid,
    this.password,
    this.openWeatherApiKey,
    this.openWeatherCity,
    this.openWeatherCountry,
    this.weatherUnits,
    this.timeZone,
    this.language,
    this.clockDuration,
    this.weatherDuration,
    this.brightness,
    this.flipDisplay,
    this.twelveHourToggle,
    this.showDayOfWeek,
    this.showDate,
    this.showHumidity,
    this.colonBlinkEnabled,
    this.showWeatherDescription,
    this.ntpServer1,
    this.ntpServer2,
    this.dimmingEnabled,
    this.dimStartHour,
    this.dimStartMinute,
    this.dimEndHour,
    this.dimEndMinute,
    this.dimBrightness,
    this.countdownEnabled,
    this.countdownTargetTimestamp,
    this.countdownLabel,
    this.isDramaticCountdown,
  });

  factory Config.fromJson(Map<String, dynamic> json) {
    return Config(
      ssid: json['ssid'],
      password: json['password'],
      openWeatherApiKey: json['openWeatherApiKey'],
      openWeatherCity: json['openWeatherCity'],
      openWeatherCountry: json['openWeatherCountry'],
      weatherUnits: json['weatherUnits'],
      timeZone: json['timeZone'],
      language: json['language'],
      clockDuration: json['clockDuration'] != null ? (json['clockDuration'] as num).toInt() : null,
      weatherDuration: json['weatherDuration'] != null ? (json['weatherDuration'] as num).toInt() : null,
      brightness: json['brightness'] != null ? (json['brightness'] as num).toInt() : null,
      flipDisplay: json['flipDisplay'],
      twelveHourToggle: json['twelveHourToggle'],
      showDayOfWeek: json['showDayOfWeek'],
      showDate: json['showDate'],
      showHumidity: json['showHumidity'],
      colonBlinkEnabled: json['colonBlinkEnabled'],
      showWeatherDescription: json['showWeatherDescription'],
      ntpServer1: json['ntpServer1'],
      ntpServer2: json['ntpServer2'],
      dimmingEnabled: json['dimmingEnabled'],
      dimStartHour: json['dimStartHour'] != null ? (json['dimStartHour'] as num).toInt() : null,
      dimStartMinute: json['dimStartMinute'] != null ? (json['dimStartMinute'] as num).toInt() : null,
      dimEndHour: json['dimEndHour'] != null ? (json['dimEndHour'] as num).toInt() : null,
      dimEndMinute: json['dimEndMinute'] != null ? (json['dimEndMinute'] as num).toInt() : null,
      dimBrightness: json['dimBrightness'] != null ? (json['dimBrightness'] as num).toInt() : null,
      countdownEnabled: json['countdown'] != null ? json['countdown']['enabled'] : null,
      countdownTargetTimestamp: json['countdown'] != null ? (json['countdown']['targetTimestamp'] as num).toInt() : null,
      countdownLabel: json['countdown'] != null ? json['countdown']['label'] : null,
      isDramaticCountdown: json['countdown'] != null ? json['countdown']['isDramaticCountdown'] : null,
    );
  }

  Map<String, dynamic> toJson() {
    final Map<String, dynamic> data = {
      'ssid': ssid,
      'password': password,
      'openWeatherApiKey': openWeatherApiKey,
      'openWeatherCity': openWeatherCity,
      'openWeatherCountry': openWeatherCountry,
      'weatherUnits': weatherUnits,
      'timeZone': timeZone,
      'language': language,
      'clockDuration': clockDuration,
      'weatherDuration': weatherDuration,
      'brightness': brightness,
      'flipDisplay': flipDisplay,
      'twelveHourToggle': twelveHourToggle,
      'showDayOfWeek': showDayOfWeek,
      'showDate': showDate,
      'showHumidity': showHumidity,
      'colonBlinkEnabled': colonBlinkEnabled,
      'showWeatherDescription': showWeatherDescription,
      'ntpServer1': ntpServer1,
      'ntpServer2': ntpServer2,
      'dimmingEnabled': dimmingEnabled,
      'dimStartHour': dimStartHour,
      'dimStartMinute': dimStartMinute,
      'dimEndHour': dimEndHour,
      'dimEndMinute': dimEndMinute,
      'dimBrightness': dimBrightness,
    };
    
    // Handle nested countdown object
    if (countdownEnabled != null || countdownTargetTimestamp != null || countdownLabel != null || isDramaticCountdown != null) {
      data['countdown'] = {
        'enabled': countdownEnabled,
        'targetTimestamp': countdownTargetTimestamp,
        'label': countdownLabel,
        'isDramaticCountdown': isDramaticCountdown,
      };
    }
    return data;
  }

  Config copyWith({
    String? ssid,
    String? password,
    String? openWeatherApiKey,
    String? openWeatherCity,
    String? openWeatherCountry,
    String? weatherUnits,
    String? timeZone,
    String? language,
    int? clockDuration,
    int? weatherDuration,
    int? brightness,
    bool? flipDisplay,
    bool? twelveHourToggle,
    bool? showDayOfWeek,
    bool? showDate,
    bool? showHumidity,
    bool? colonBlinkEnabled,
    bool? showWeatherDescription,
    String? ntpServer1,
    String? ntpServer2,
    bool? dimmingEnabled,
    int? dimStartHour,
    int? dimStartMinute,
    int? dimEndHour,
    int? dimEndMinute,
    int? dimBrightness,
    bool? countdownEnabled,
    int? countdownTargetTimestamp,
    String? countdownLabel,
    bool? isDramaticCountdown,
  }) {
    return Config(
      ssid: ssid ?? this.ssid,
      password: password ?? this.password,
      openWeatherApiKey: openWeatherApiKey ?? this.openWeatherApiKey,
      openWeatherCity: openWeatherCity ?? this.openWeatherCity,
      openWeatherCountry: openWeatherCountry ?? this.openWeatherCountry,
      weatherUnits: weatherUnits ?? this.weatherUnits,
      timeZone: timeZone ?? this.timeZone,
      language: language ?? this.language,
      clockDuration: clockDuration ?? this.clockDuration,
      weatherDuration: weatherDuration ?? this.weatherDuration,
      brightness: brightness ?? this.brightness,
      flipDisplay: flipDisplay ?? this.flipDisplay,
      twelveHourToggle: twelveHourToggle ?? this.twelveHourToggle,
      showDayOfWeek: showDayOfWeek ?? this.showDayOfWeek,
      showDate: showDate ?? this.showDate,
      showHumidity: showHumidity ?? this.showHumidity,
      colonBlinkEnabled: colonBlinkEnabled ?? this.colonBlinkEnabled,
      showWeatherDescription: showWeatherDescription ?? this.showWeatherDescription,
      ntpServer1: ntpServer1 ?? this.ntpServer1,
      ntpServer2: ntpServer2 ?? this.ntpServer2,
      dimmingEnabled: dimmingEnabled ?? this.dimmingEnabled,
      dimStartHour: dimStartHour ?? this.dimStartHour,
      dimStartMinute: dimStartMinute ?? this.dimStartMinute,
      dimEndHour: dimEndHour ?? this.dimEndHour,
      dimEndMinute: dimEndMinute ?? this.dimEndMinute,
      dimBrightness: dimBrightness ?? this.dimBrightness,
      countdownEnabled: countdownEnabled ?? this.countdownEnabled,
      countdownTargetTimestamp: countdownTargetTimestamp ?? this.countdownTargetTimestamp,
      countdownLabel: countdownLabel ?? this.countdownLabel,
      isDramaticCountdown: isDramaticCountdown ?? this.isDramaticCountdown,
    );
  }
}