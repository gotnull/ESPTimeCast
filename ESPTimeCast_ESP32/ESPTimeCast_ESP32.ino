#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <sntp.h>
#include <time.h>
#include <WiFiClientSecure.h>

#include "mfactoryfont.h"
#include "tz_lookup.h"
#include "days_lookup.h"
#include "months_lookup.h"

// Disable debug prints to save ~20KB
//#define DEBUG_SERIAL
#ifdef DEBUG_SERIAL
  #define DEBUG_PRINT(x) DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x) DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...) DEBUG_PRINTF(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 2
#define CS_PIN 3
#define DATA_PIN 4

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
AsyncWebServer server(80);

// --- Global Scroll Speed Settings ---

// WiFi and configuration globals
char ssid[32] = "";
char password[32] = "";
char openWeatherApiKey[64] = "";
char openWeatherCity[64] = "";
char openWeatherCountry[64] = "";
char weatherUnits[12] = "metric";
char timeZone[64] = "";
char language[8] = "en";
String mainDesc = "";
String detailedDesc = "";

// Timing and display settings
unsigned long clockDuration = 10000;
unsigned long weatherDuration = 5000;
unsigned long demoDuration = 10000; // 10 seconds for demoscene
int brightness = 7;
bool flipDisplay = false;
bool twelveHourToggle = false;
bool showDayOfWeek = true;
bool showDate = false;
bool showHumidity = false;
bool colonBlinkEnabled = true;
char ntpServer1[64] = "pool.ntp.org";
char ntpServer2[256] = "time.nist.gov";
int scrollSpeed = 100;

// Dimming
bool dimmingEnabled = false;
int dimStartHour = 18; // 6pm default
int dimStartMinute = 0;
int dimEndHour = 8; // 8am default
int dimEndMinute = 0;
int dimBrightness = 2; // Dimming level (0-15)

// Countdown Globals - NEW
bool countdownEnabled = false;
time_t countdownTargetTimestamp = 0; // Unix timestamp
char countdownLabel[64] = "";        // Label for the countdown
bool isDramaticCountdown = true;     // Default to the dramatic countdown mode

// State management
bool weatherCycleStarted = false;
WiFiClient client;
const byte DNS_PORT = 53;
DNSServer dnsServer;

String currentTemp = "";
String weatherDescription = "";
bool showWeatherDescription = false;
bool weatherAvailable = false;
bool weatherFetched = false;
bool weatherFetchInitiated = false;
bool isAPMode = false;
char tempSymbol = '[';
bool shouldFetchWeatherNow = false;

unsigned long lastSwitch = 0;
unsigned long lastColonBlink = 0;
int displayMode = 0; // 0: Clock, 1: Weather, 2: Weather Description, 3: Countdown, 4: Nightscout, 5: Date, 6: Demoscene
int currentHumidity = -1;
bool ntpSyncSuccessful = false;

// NTP Synchronization State Machine
enum NtpState
{
  NTP_IDLE,
  NTP_SYNCING,
  NTP_SUCCESS,
  NTP_FAILED
};
NtpState ntpState = NTP_IDLE;
unsigned long ntpStartTime = 0;
const int ntpTimeout = 30000; // 30 seconds
const int maxNtpRetries = 30;
int ntpRetryCount = 0;
unsigned long lastNtpStatusPrintTime = 0;
const unsigned long ntpStatusPrintInterval = 1000; // Print status every 1 seconds (adjust as needed)

// Non-blocking IP display globals
bool showingIp = false;
int ipDisplayCount = 0;
const int ipDisplayMax = 2; // As per working copy for how long IP shows
String pendingIpToShow = "";

// Countdown display state - NEW
bool countdownScrolling = false;
unsigned long countdownScrollEndTime = 0;
unsigned long countdownStaticStartTime = 0; // For last-day static display

// --- NEW GLOBAL VARIABLES FOR IMMEDIATE COUNTDOWN FINISH ---
bool countdownFinished = false;                      // Tracks if the countdown has permanently finished
bool countdownShowFinishedMessage = false;           // Flag to indicate "TIMES UP" message is active
unsigned long countdownFinishedMessageStartTime = 0; // Timer for the 10-second message duration
unsigned long lastFlashToggleTime = 0;               // For controlling the flashing speed
bool currentInvertState = false;                     // Current state of display inversion for flashing
static bool hourglassPlayed = false;

// Weather Description Mode handling
unsigned long descStartTime = 0; // For static description
bool descScrolling = false;
const unsigned long descriptionDuration = 3000;   // 3s for short text
static unsigned long descScrollEndTime = 0;       // for post-scroll delay (re-used for scroll timing)
const unsigned long descriptionScrollPause = 300; // 300ms pause after scroll

// Scroll flipped
textEffect_t getEffectiveScrollDirection(textEffect_t desiredDirection, bool isFlipped)
{
  if (isFlipped)
  {
    // If the display is horizontally flipped, reverse the horizontal scroll direction
    if (desiredDirection == PA_SCROLL_LEFT)
    {
      return PA_SCROLL_RIGHT;
    }
    else if (desiredDirection == PA_SCROLL_RIGHT)
    {
      return PA_SCROLL_LEFT;
    }
  }
  return desiredDirection;
}

// -----------------------------------------------------------------------------
// Configuration Load & Save
// -----------------------------------------------------------------------------
void loadConfig()
{
  DEBUG_PRINTLN(F("[CONFIG] Loading configuration..."));

  // Check if config.json exists, if not, create default
  if (!LittleFS.exists("/config.json"))
  {
    DEBUG_PRINTLN(F("[CONFIG] config.json not found, creating with defaults..."));
    DynamicJsonDocument doc(2048);
    doc[F("ssid")] = "";
    doc[F("password")] = "";
    doc[F("openWeatherApiKey")] = "";
    doc[F("openWeatherCity")] = "";
    doc[F("openWeatherCountry")] = "";
    doc[F("weatherUnits")] = "metric";
    doc[F("clockDuration")] = 10000;
    doc[F("weatherDuration")] = 5000;
    doc[F("timeZone")] = "";
    doc[F("language")] = "en";
    doc[F("brightness")] = brightness;
    doc[F("flipDisplay")] = flipDisplay;
    doc[F("twelveHourToggle")] = twelveHourToggle;
    doc[F("showDayOfWeek")] = showDayOfWeek;
    doc[F("showDate")] = false;
    doc[F("showHumidity")] = showHumidity;
    doc[F("colonBlinkEnabled")] = colonBlinkEnabled;
    doc[F("ntpServer1")] = ntpServer1;
    doc[F("ntpServer2")] = ntpServer2;
    doc[F("dimmingEnabled")] = dimmingEnabled;
    doc[F("dimStartHour")] = dimStartHour;
    doc[F("dimStartMinute")] = dimStartMinute;
    doc[F("dimEndHour")] = dimEndHour;
    doc[F("dimEndMinute")] = dimEndMinute;
    doc[F("dimBrightness")] = dimBrightness;
    doc[F("showWeatherDescription")] = showWeatherDescription;

    // Add countdown defaults when creating a new config.json
    JsonObject countdownObj = doc.createNestedObject("countdown");
    countdownObj["enabled"] = false;
    countdownObj["targetTimestamp"] = 0;
    countdownObj["label"] = "";
    countdownObj["isDramaticCountdown"] = true;

    File f = LittleFS.open("/config.json", "w");
    if (f)
    {
      serializeJsonPretty(doc, f);
      f.close();
      DEBUG_PRINTLN(F("[CONFIG] Default config.json created."));
    }
    else
    {
      DEBUG_PRINTLN(F("[ERROR] Failed to create default config.json"));
    }
  }

  DEBUG_PRINTLN(F("[CONFIG] Attempting to open config.json for reading."));
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    DEBUG_PRINTLN(F("[ERROR] Failed to open config.json for reading. Cannot load config."));
    return;
  }

  DynamicJsonDocument doc(2048); // Size based on ArduinoJson Assistant + buffer
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error)
  {
    DEBUG_PRINT(F("[ERROR] JSON parse failed during load: "));
    DEBUG_PRINTLN(error.f_str());
    return;
  }

  strlcpy(ssid, doc["ssid"] | "", sizeof(ssid));
  strlcpy(password, doc["password"] | "", sizeof(password));
  strlcpy(openWeatherApiKey, doc["openWeatherApiKey"] | "", sizeof(openWeatherApiKey)); // Corrected typo here
  strlcpy(openWeatherCity, doc["openWeatherCity"] | "", sizeof(openWeatherCity));
  strlcpy(openWeatherCountry, doc["openWeatherCountry"] | "", sizeof(openWeatherCountry));
  strlcpy(weatherUnits, doc["weatherUnits"] | "metric", sizeof(weatherUnits));
  clockDuration = doc["clockDuration"] | 10000;
  weatherDuration = doc["weatherDuration"] | 5000;
  strlcpy(timeZone, doc["timeZone"] | "Etc/UTC", sizeof(timeZone));
  if (doc.containsKey("language"))
  {
    strlcpy(language, doc["language"], sizeof(language));
  }
  else
  {
    strlcpy(language, "en", sizeof(language));
    DEBUG_PRINTLN(F("[CONFIG] 'language' key not found in config.json, defaulting to 'en'."));
  }

  brightness = doc["brightness"] | 7;
  flipDisplay = doc["flipDisplay"] | false;
  twelveHourToggle = doc["twelveHourToggle"] | false;
  showDayOfWeek = doc["showDayOfWeek"] | true;
  showDate = doc["showDate"] | false;
  showHumidity = doc["showHumidity"] | false;
  colonBlinkEnabled = doc.containsKey("colonBlinkEnabled") ? doc["colonBlinkEnabled"].as<bool>() : true;
  showWeatherDescription = doc["showWeatherDescription"] | false;

  String de = doc["dimmingEnabled"].as<String>();
  dimmingEnabled = (de == "true" || de == "on" || de == "1");

  dimStartHour = doc["dimStartHour"] | 18;
  dimStartMinute = doc["dimStartMinute"] | 0;
  dimEndHour = doc["dimEndHour"] | 8;
  dimEndMinute = doc["dimEndMinute"] | 0;
  dimBrightness = doc["dimBrightness"] | 0;

  strlcpy(ntpServer1, doc["ntpServer1"] | "pool.ntp.org", sizeof(ntpServer1));
  strlcpy(ntpServer2, doc["ntpServer2"] | "time.nist.gov", sizeof(ntpServer2));
  scrollSpeed = doc["scrollSpeed"] | 100;

  if (strcmp(weatherUnits, "imperial") == 0)
    tempSymbol = ']';
  else
    tempSymbol = '[';

  // --- COUNTDOWN CONFIG LOADING ---
  if (doc.containsKey("countdown"))
  {
    JsonObject countdownObj = doc["countdown"];

    countdownEnabled = countdownObj["enabled"] | false;
    countdownTargetTimestamp = countdownObj["targetTimestamp"] | 0;
    isDramaticCountdown = countdownObj["isDramaticCountdown"] | true;

    JsonVariant labelVariant = countdownObj["label"];
    if (labelVariant.isNull() || !labelVariant.is<const char *>())
    {
      strcpy(countdownLabel, "");
    }
    else
    {
      const char *labelTemp = labelVariant.as<const char *>();
      size_t labelLen = strlen(labelTemp);
      if (labelLen >= sizeof(countdownLabel))
      {
        DEBUG_PRINTLN(F("[CONFIG] label from JSON too long, truncating."));
      }
      strlcpy(countdownLabel, labelTemp, sizeof(countdownLabel));
    }
    countdownFinished = false;
  }
  else
  {
    countdownEnabled = false;
    countdownTargetTimestamp = 0;
    strcpy(countdownLabel, "");
    isDramaticCountdown = true;
    DEBUG_PRINTLN(F("[CONFIG] Countdown object not found, defaulting to disabled."));
    countdownFinished = false;
  }
  DEBUG_PRINTLN(F("[CONFIG] Configuration loaded."));
}

// -----------------------------------------------------------------------------
// WiFi Setup
// -----------------------------------------------------------------------------
const char *DEFAULT_AP_PASSWORD = "12345678";
const char *AP_SSID = "ESPTimeCast";

void connectWiFi()
{
  DEBUG_PRINTLN(F("[WIFI] Connecting to WiFi..."));

  bool credentialsExist = (strlen(ssid) > 0);

  if (!credentialsExist)
  {
    DEBUG_PRINTLN(F("[WIFI] No saved credentials. Starting AP mode directly."));
    WiFi.mode(WIFI_AP);
    WiFi.disconnect(true);
    delay(100);

    if (strlen(DEFAULT_AP_PASSWORD) < 8)
    {
      WiFi.softAP(AP_SSID);
      DEBUG_PRINTLN(F("[WIFI] AP Mode started (no password, too short)."));
    }
    else
    {
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      DEBUG_PRINTLN(F("[WIFI] AP Mode started."));
    }

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    DEBUG_PRINT(F("[WIFI] AP IP address: "));
    DEBUG_PRINTLN(WiFi.softAPIP());
    isAPMode = true;

    clearWiFiCredentialsInConfig();
    strlcpy(ssid, "", sizeof(ssid));
    strlcpy(password, "", sizeof(password));

    WiFiMode_t mode = WiFi.getMode();
    DEBUG_PRINTF("[WIFI] WiFi mode after setting AP: %s\n",
                  mode == WIFI_OFF ? "OFF" : mode == WIFI_STA  ? "STA ONLY"
                                         : mode == WIFI_AP     ? "AP ONLY"
                                         : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                               : "UNKNOWN");

    DEBUG_PRINTLN(F("[WIFI] AP Mode Started"));
    return;
  }

  // If credentials exist, attempt STA connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  const unsigned long timeout = 30000;
  unsigned long animTimer = 0;
  int animFrame = 0;
  bool animating = true;

  while (animating)
  {
    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED)
    {
      DEBUG_PRINTLN("[WiFi] Connected: " + WiFi.localIP().toString());
      isAPMode = false;

      WiFiMode_t mode = WiFi.getMode();
      DEBUG_PRINTF("[WIFI] WiFi mode after STA connection: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA  ? "STA ONLY"
                                           : mode == WIFI_AP     ? "AP ONLY"
                                           : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                 : "UNKNOWN");

      // --- IP Display initiation ---
      pendingIpToShow = WiFi.localIP().toString();
      showingIp = true;
      ipDisplayCount = 0; // Reset count for IP display
      P.displayClear();
      P.setCharSpacing(1); // Set spacing for IP scroll
      textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
      P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, scrollSpeed);
      // --- END IP Display initiation ---

      animating = false; // Exit the connection loop
      break;
    }
    else if (now - startAttemptTime >= timeout)
    {
      DEBUG_PRINTLN(F("[WiFi] Failed. Starting AP mode..."));
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      DEBUG_PRINT(F("[WiFi] AP IP address: "));
      DEBUG_PRINTLN(WiFi.softAPIP());
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;

      clearWiFiCredentialsInConfig();
      strlcpy(ssid, "", sizeof(ssid));
      strlcpy(password, "", sizeof(password));

      auto mode = WiFi.getMode();
      DEBUG_PRINTF("[WIFI] WiFi mode after STA failure and setting AP: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA  ? "STA ONLY"
                                           : mode == WIFI_AP     ? "AP ONLY"
                                           : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                 : "UNKNOWN");

      animating = false;
      DEBUG_PRINTLN(F("[WIFI] AP Mode Started"));
      break;
    }
    if (now - animTimer > 750)
    {
      animTimer = now;
      P.setTextAlignment(PA_CENTER);
      switch (animFrame % 3)
      {
      case 0:
        P.print(F("# ©"));
        break;
      case 1:
        P.print(F("# ª"));
        break;
      case 2:
        P.print(F("# «"));
        break;
      }
      animFrame++;
    }
    delay(1);
  }
}

void clearWiFiCredentialsInConfig()
{
  DynamicJsonDocument doc(2048);

  // Open existing config, if present
  File configFile = LittleFS.open("/config.json", "r");
  if (configFile)
  {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err)
    {
      DEBUG_PRINT(F("[SECURITY] Error parsing config.json: "));
      DEBUG_PRINTLN(err.f_str());
      return;
    }
  }

  doc["ssid"] = "";
  doc["password"] = "";

  // Optionally backup previous config
  if (LittleFS.exists("/config.json"))
  {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f)
  {
    DEBUG_PRINTLN(F("[SECURITY] ERROR: Cannot write to /config.json to clear credentials!"));
    return;
  }
  serializeJson(doc, f);
  f.close();
  DEBUG_PRINTLN(F("[SECURITY] Cleared WiFi credentials in config.json."));
}

// -----------------------------------------------------------------------------
// Time / NTP Functions
// -----------------------------------------------------------------------------
void setupTime()
{
  if (!isAPMode)
  {
    DEBUG_PRINTLN(F("[TIME] Starting NTP sync"));
  }

  bool serverOk = false;
  IPAddress resolvedIP;

  // Try first server if it's not empty
  if (strlen(ntpServer1) > 0 && WiFi.hostByName(ntpServer1, resolvedIP) == 1)
  {
    serverOk = true;
  }
  // Try second server if first failed
  else if (strlen(ntpServer2) > 0 && WiFi.hostByName(ntpServer2, resolvedIP) == 1)
  {
    serverOk = true;
  }

  if (serverOk)
  {
    configTime(0, 0, ntpServer1, ntpServer2); // safe to call now
    setenv("TZ", ianaToPosix(timeZone), 1);
    tzset();
    ntpState = NTP_SYNCING;
    ntpStartTime = millis();
    ntpRetryCount = 0;
    ntpSyncSuccessful = false;
  }
  else
  {
    DEBUG_PRINTLN(F("[TIME] NTP server lookup failed — retry sync in 30 seconds"));
    ntpSyncSuccessful = false;
    ntpState = NTP_SYNCING;  // instead of NTP_IDLE
    ntpStartTime = millis(); // start the failed timer (so retry delay counts from now)
  }
}

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------
void printConfigToSerial()
{
  DEBUG_PRINTLN(F("========= Loaded Configuration ========="));
  DEBUG_PRINT(F("WiFi SSID: "));
  DEBUG_PRINTLN(ssid);
  DEBUG_PRINT(F("WiFi Password: "));
  DEBUG_PRINTLN(password);
  DEBUG_PRINT(F("OpenWeather City: "));
  DEBUG_PRINTLN(openWeatherCity);
  DEBUG_PRINT(F("OpenWeather Country: "));
  DEBUG_PRINTLN(openWeatherCountry);
  DEBUG_PRINT(F("OpenWeather API Key: "));
  DEBUG_PRINTLN(openWeatherApiKey);
  DEBUG_PRINT(F("Temperature Unit: "));
  DEBUG_PRINTLN(weatherUnits);
  DEBUG_PRINT(F("Clock duration: "));
  DEBUG_PRINTLN(clockDuration);
  DEBUG_PRINT(F("Weather duration: "));
  DEBUG_PRINTLN(weatherDuration);
  DEBUG_PRINT(F("TimeZone (IANA): "));
  DEBUG_PRINTLN(timeZone);
  DEBUG_PRINT(F("Days of the Week/Weather description language: "));
  DEBUG_PRINTLN(language);
  DEBUG_PRINT(F("Brightness: "));
  DEBUG_PRINTLN(brightness);
  DEBUG_PRINT(F("Flip Display: "));
  DEBUG_PRINTLN(flipDisplay ? "Yes" : "No");
  DEBUG_PRINT(F("Show 12h Clock: "));
  DEBUG_PRINTLN(twelveHourToggle ? "Yes" : "No");
  DEBUG_PRINT(F("Show Day of the Week: "));
  DEBUG_PRINTLN(showDayOfWeek ? "Yes" : "No");
  DEBUG_PRINT(F("Show Date: "));
  DEBUG_PRINTLN(showDate ? "Yes" : "No");
  DEBUG_PRINT(F("Show Weather Description: "));
  DEBUG_PRINTLN(showWeatherDescription ? "Yes" : "No");
  DEBUG_PRINT(F("Show Humidity: "));
  DEBUG_PRINTLN(showHumidity ? "Yes" : "No");
  DEBUG_PRINT(F("Blinking colon: "));
  DEBUG_PRINTLN(colonBlinkEnabled ? "Yes" : "No");
  DEBUG_PRINT(F("NTP Server 1: "));
  DEBUG_PRINTLN(ntpServer1);
  DEBUG_PRINT(F("NTP Server 2: "));
  DEBUG_PRINTLN(ntpServer2);
  DEBUG_PRINT(F("Dimming Enabled: "));
  DEBUG_PRINTLN(dimmingEnabled);
  DEBUG_PRINT(F("Dimming Start Hour: "));
  DEBUG_PRINTLN(dimStartHour);
  DEBUG_PRINT(F("Dimming Start Minute: "));
  DEBUG_PRINTLN(dimStartMinute);
  DEBUG_PRINT(F("Dimming End Hour: "));
  DEBUG_PRINTLN(dimEndHour);
  DEBUG_PRINT(F("Dimming End Minute: "));
  DEBUG_PRINTLN(dimEndMinute);
  DEBUG_PRINT(F("Dimming Brightness: "));
  DEBUG_PRINTLN(dimBrightness);
  DEBUG_PRINT(F("Countdown Enabled: "));
  DEBUG_PRINTLN(countdownEnabled ? "Yes" : "No");
  DEBUG_PRINT(F("Countdown Target Timestamp: "));
  DEBUG_PRINTLN(countdownTargetTimestamp);
  DEBUG_PRINT(F("Countdown Label: "));
  DEBUG_PRINTLN(countdownLabel);
  DEBUG_PRINT(F("Dramatic Countdown Display: "));
  DEBUG_PRINTLN(isDramaticCountdown ? "Yes" : "No");
  DEBUG_PRINTLN(F("========================================"));
  DEBUG_PRINTLN();
}

// -----------------------------------------------------------------------------
// Web Server and Captive Portal
// -----------------------------------------------------------------------------
void handleCaptivePortal(AsyncWebServerRequest *request);

// Helper to parse boolean from request
bool getBoolParam(AsyncWebServerRequest *request) {
  if (!request->hasParam("value", true)) return false;
  String v = request->getParam("value", true)->value();
  return (v == "1" || v == "true" || v == "on");
}

void setupWebServer()
{
  DEBUG_PRINTLN(F("[WEBSERVER] Setting up web server..."));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    DEBUG_PRINTLN(F("[WEBSERVER] Request: /"));
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      DEBUG_PRINTLN(F("[WEBSERVER] index.html not found in LittleFS"));
      request->send(404, "text/plain", "index.html not found. Upload data folder to LittleFS.");
    } });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    DEBUG_PRINTLN(F("[WEBSERVER] Request: /config.json"));
    File f = LittleFS.open("/config.json", "r");
    if (!f) {
      DEBUG_PRINTLN(F("[WEBSERVER] Error opening /config.json"));
      request->send(500, "application/json", "{\"error\":\"Failed to open config.json\"}");
      return;
    }
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      DEBUG_PRINT(F("[WEBSERVER] Error parsing /config.json: "));
      DEBUG_PRINTLN(err.f_str());
      request->send(500, "application/json", "{\"error\":\"Failed to parse config.json\"}");
      return;
    }
    doc[F("mode")] = isAPMode ? "ap" : "sta";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    DEBUG_PRINTLN(F("[WEBSERVER] Request: /save"));
    DynamicJsonDocument doc(2048);

    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      DEBUG_PRINTLN(F("[WEBSERVER] Existing config.json found, loading for update..."));
      DeserializationError err = deserializeJson(doc, configFile);
      configFile.close();
      if (err) {
        DEBUG_PRINT(F("[WEBSERVER] Error parsing existing config.json: "));
        DEBUG_PRINTLN(err.f_str());
      }
    } else {
      DEBUG_PRINTLN(F("[WEBSERVER] config.json not found, starting with empty doc for save."));
    }

    for (int i = 0; i < request->params(); i++) {
      const AsyncWebParameter *p = request->getParam(i);
      String n = p->name();
      String v = p->value();

      if (n == "brightness") doc[n] = v.toInt();
      else if (n == "clockDuration") doc[n] = v.toInt();
      else if (n == "weatherDuration") doc[n] = v.toInt();
      else if (n == "flipDisplay") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "twelveHourToggle") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showDayOfWeek") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showDate") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showHumidity") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "colonBlinkEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "dimStartHour") doc[n] = v.toInt();
      else if (n == "dimStartMinute") doc[n] = v.toInt();
      else if (n == "dimEndHour") doc[n] = v.toInt();
      else if (n == "dimEndMinute") doc[n] = v.toInt();
      else if (n == "dimBrightness") doc[n] = v.toInt();
      else if (n == "showWeatherDescription") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "dimmingEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "weatherUnits") doc[n] = v;
      else {
        doc[n] = v;
      }
    }

    bool newCountdownEnabled = (request->hasParam("countdownEnabled", true) && (request->getParam("countdownEnabled", true)->value() == "true" || request->getParam("countdownEnabled", true)->value() == "on" || request->getParam("countdownEnabled", true)->value() == "1"));
    String countdownDateStr = request->hasParam("countdownDate", true) ? request->getParam("countdownDate", true)->value() : "";
    String countdownTimeStr = request->hasParam("countdownTime", true) ? request->getParam("countdownTime", true)->value() : "";
    String countdownLabelStr = request->hasParam("countdownLabel", true) ? request->getParam("countdownLabel", true)->value() : "";
    bool newIsDramaticCountdown = (request->hasParam("isDramaticCountdown", true) && (request->getParam("isDramaticCountdown", true)->value() == "true" || request->getParam("isDramaticCountdown", true)->value() == "on" || request->getParam("isDramaticCountdown", true)->value() == "1"));

    time_t newTargetTimestamp = 0;
    if (newCountdownEnabled && countdownDateStr.length() > 0 && countdownTimeStr.length() > 0) {
      int year = countdownDateStr.substring(0, 4).toInt();
      int month = countdownDateStr.substring(5, 7).toInt();
      int day = countdownDateStr.substring(8, 10).toInt();
      int hour = countdownTimeStr.substring(0, 2).toInt();
      int minute = countdownTimeStr.substring(3, 5).toInt();

      struct tm tm;
      tm.tm_year = year - 1900;
      tm.tm_mon = month - 1;
      tm.tm_mday = day;
      tm.tm_hour = hour;
      tm.tm_min = minute;
      tm.tm_sec = 0;
      tm.tm_isdst = -1;

      newTargetTimestamp = mktime(&tm);
      if (newTargetTimestamp == (time_t)-1) {
        DEBUG_PRINTLN("[SAVE] Error converting countdown date/time to timestamp.");
        newTargetTimestamp = 0;
      } else {
        DEBUG_PRINTF("[SAVE] Converted countdown target: %s -> %lu\n", countdownDateStr.c_str(), newTargetTimestamp);
      }
    }

    JsonObject countdownObj = doc.createNestedObject("countdown");
    countdownObj["enabled"] = newCountdownEnabled;
    countdownObj["targetTimestamp"] = newTargetTimestamp;
    countdownObj["label"] = countdownLabelStr;
    countdownObj["isDramaticCountdown"] = newIsDramaticCountdown;

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    DEBUG_PRINTF("[SAVE] LittleFS total bytes: %llu, used bytes: %llu\n", LittleFS.totalBytes(), LittleFS.usedBytes());

    if (LittleFS.exists("/config.json")) {
      DEBUG_PRINTLN(F("[SAVE] Renaming /config.json to /config.bak"));
      LittleFS.rename("/config.json", "/config.bak");
    }
    File f = LittleFS.open("/config.json", "w");
    if (!f) {
      DEBUG_PRINTLN(F("[SAVE] ERROR: Failed to open /config.json for writing!"));
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = "Failed to write config file.";
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    size_t bytesWritten = serializeJson(doc, f);
    DEBUG_PRINTF("[SAVE] Bytes written to /config.json: %u\n", bytesWritten);
    f.close();
    DEBUG_PRINTLN(F("[SAVE] /config.json file closed."));

    File verify = LittleFS.open("/config.json", "r");
    if (!verify) {
      DEBUG_PRINTLN(F("[SAVE] ERROR: Failed to open /config.json for reading during verification!"));
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = "Verification failed: Could not re-open config file.";
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    while (verify.available()) {
      verify.read();
    }
    verify.seek(0);

    DynamicJsonDocument test(2048);
    DeserializationError err = deserializeJson(test, verify);
    verify.close();

    if (err) {
      DEBUG_PRINT(F("[SAVE] Config corrupted after save: "));
      DEBUG_PRINTLN(err.f_str());
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = String("Config corrupted. Reboot cancelled. Error: ") + err.f_str();
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    DEBUG_PRINTLN(F("[SAVE] Config verification successful."));
    DynamicJsonDocument okDoc(128);
    okDoc[F("message")] = "Saved successfully. Rebooting...";
    String response;
    serializeJson(okDoc, response);
    request->send(200, "application/json", response);
    DEBUG_PRINTLN(F("[WEBSERVER] Sending success response and scheduling reboot..."));

    request->onDisconnect([]() {
      DEBUG_PRINTLN(F("[WEBSERVER] Client disconnected, rebooting ESP..."));
      ESP.restart();
    }); });

  server.on("/restore", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    DEBUG_PRINTLN(F("[WEBSERVER] Request: /restore"));
    if (LittleFS.exists("/config.bak")) {
      File src = LittleFS.open("/config.bak", "r");
      if (!src) {
        DEBUG_PRINTLN(F("[WEBSERVER] Failed to open /config.bak"));
        DynamicJsonDocument errorDoc(128);
        errorDoc[F("error")] = "Failed to open backup file.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }
      File dst = LittleFS.open("/config.json", "w");
      if (!dst) {
        src.close();
        DEBUG_PRINTLN(F("[WEBSERVER] Failed to open /config.json for writing"));
        DynamicJsonDocument errorDoc(128);
        errorDoc[F("error")] = "Failed to open config for writing.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }

      while (src.available()) {
        dst.write(src.read());
      }
      src.close();
      dst.close();

      DynamicJsonDocument okDoc(128);
      okDoc[F("message")] = "✅ Backup restored! Device will now reboot.";
      String response;
      serializeJson(okDoc, response);
      request->send(200, "application/json", response);
      request->onDisconnect([]() {
        DEBUG_PRINTLN(F("[WEBSERVER] Rebooting after restore..."));
        ESP.restart();
      });

    } else {
      DEBUG_PRINTLN(F("[WEBSERVER] No backup found"));
      DynamicJsonDocument errorDoc(128);
      errorDoc[F("error")] = "No backup found.";
      String response;
      serializeJson(errorDoc, response);
      request->send(404, "application/json", response);
    } });

  server.on("/ap_status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    DEBUG_PRINT(F("[WEBSERVER] Request: /ap_status. isAPMode = "));
    DEBUG_PRINTLN(isAPMode);
    String json = "{\"isAP\": ";
    json += (isAPMode) ? "true" : "false";
    json += "}";
    request->send(200, "application/json", json); });

  server.on("/set_brightness", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }
    int newBrightness = request->getParam("value", true)->value().toInt();
    if (newBrightness < 0) newBrightness = 0;
    if (newBrightness > 15) newBrightness = 15;
    brightness = newBrightness;
    P.setIntensity(brightness);
    DEBUG_PRINTF("[WEBSERVER] Set brightness to %d\n", brightness);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_scroll_speed", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }
    int newScrollSpeed = request->getParam("value", true)->value().toInt();
    if (newScrollSpeed < 10) newScrollSpeed = 10;
    if (newScrollSpeed > 200) newScrollSpeed = 200;
    scrollSpeed = newScrollSpeed;
    DEBUG_PRINTF("[WEBSERVER] Set scrollSpeed to %d\n", scrollSpeed);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_flip", HTTP_POST, [](AsyncWebServerRequest *request) {
    flipDisplay = getBoolParam(request);
    P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
    P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_twelvehour", HTTP_POST, [](AsyncWebServerRequest *request) {
    twelveHourToggle = getBoolParam(request);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_dayofweek", HTTP_POST, [](AsyncWebServerRequest *request) {
    showDayOfWeek = getBoolParam(request);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_showdate", HTTP_POST, [](AsyncWebServerRequest *request) {
    showDate = getBoolParam(request);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_humidity", HTTP_POST, [](AsyncWebServerRequest *request) {
    showHumidity = getBoolParam(request);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_colon_blink", HTTP_POST, [](AsyncWebServerRequest *request) {
    colonBlinkEnabled = getBoolParam(request);
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_language", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }

    String lang = request->getParam("value", true)->value();
    lang.trim();         // Remove whitespace/newlines
    lang.toLowerCase();  // Normalize to lowercase

    strlcpy(language, lang.c_str(), sizeof(language));              // Safe copy to char[]
    DEBUG_PRINTF("[WEBSERVER] Set language to '%s'\n", language);  // Use quotes for debug

    shouldFetchWeatherNow = true;

    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_weatherdesc", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDesc = getBoolParam(request);
    if (showWeatherDescription && !showDesc && displayMode == 2) advanceDisplayMode();
    showWeatherDescription = showDesc;
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_units", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool imperial = getBoolParam(request);
    strcpy(weatherUnits, imperial ? "imperial" : "metric");
    tempSymbol = imperial ? ']' : '[';
    shouldFetchWeatherNow = true;
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_countdown_enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enable = getBoolParam(request);
    if (countdownEnabled && !enable && displayMode == 3) advanceDisplayMode();
    countdownEnabled = enable;
    request->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/set_dramatic_countdown", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool dramatic = getBoolParam(request);
    if (isDramaticCountdown != dramatic) {
      isDramaticCountdown = dramatic;
      saveCountdownConfig(countdownEnabled, countdownTargetTimestamp, countdownLabel);
    }
    request->send(200, "application/json", "{\"ok\":true}"); });

  // Captive portal - catch all requests and redirect to root
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    if (isAPMode) {
      handleCaptivePortal(request);
    } else {
      request->send(404, "text/plain", "Not Found");
    } });

  server.begin();
  DEBUG_PRINTLN(F("[WEBSERVER] Web server started"));
}

void handleCaptivePortal(AsyncWebServerRequest *request)
{
  DEBUG_PRINT(F("[WEBSERVER] Captive Portal Redirecting: "));
  DEBUG_PRINTLN(request->url());
  request->redirect(String("http://") + WiFi.softAPIP().toString() + "/");
}

String normalizeWeatherDescription(String str)
{
  // Serbian Cyrillic → Latin
  str.replace("а", "a");
  str.replace("б", "b");
  str.replace("в", "v");
  str.replace("г", "g");
  str.replace("д", "d");
  str.replace("ђ", "dj");
  str.replace("е", "e");
  str.replace("ж", "z");
  str.replace("з", "z");
  str.replace("и", "i");
  str.replace("ј", "j");
  str.replace("к", "k");
  str.replace("л", "l");
  str.replace("љ", "lj");
  str.replace("м", "m");
  str.replace("н", "n");
  str.replace("њ", "nj");
  str.replace("о", "o");
  str.replace("п", "p");
  str.replace("р", "r");
  str.replace("с", "s");
  str.replace("т", "t");
  str.replace("ћ", "c");
  str.replace("у", "u");
  str.replace("ф", "f");
  str.replace("х", "h");
  str.replace("ц", "c");
  str.replace("ч", "c");
  str.replace("џ", "dz");
  str.replace("ш", "s");

  // Latin diacritics → ASCII
  str.replace("å", "a");
  str.replace("ä", "a");
  str.replace("à", "a");
  str.replace("á", "a");
  str.replace("â", "a");
  str.replace("ã", "a");
  str.replace("ā", "a");
  str.replace("ă", "a");
  str.replace("ą", "a");

  str.replace("æ", "ae");

  str.replace("ç", "c");
  str.replace("č", "c");
  str.replace("ć", "c");

  str.replace("ď", "d");

  str.replace("é", "e");
  str.replace("è", "e");
  str.replace("ê", "e");
  str.replace("ë", "e");
  str.replace("ē", "e");
  str.replace("ė", "e");
  str.replace("ę", "e");

  str.replace("ğ", "g");
  str.replace("ģ", "g");

  str.replace("ĥ", "h");

  str.replace("í", "i");
  str.replace("ì", "i");
  str.replace("î", "i");
  str.replace("ï", "i");
  str.replace("ī", "i");
  str.replace("į", "i");

  str.replace("ĵ", "j");

  str.replace("ķ", "k");

  str.replace("ľ", "l");
  str.replace("ł", "l");

  str.replace("ñ", "n");
  str.replace("ń", "n");
  str.replace("ņ", "n");

  str.replace("ó", "o");
  str.replace("ò", "o");
  str.replace("ô", "o");
  str.replace("ö", "o");
  str.replace("õ", "o");
  str.replace("ø", "o");
  str.replace("ō", "o");
  str.replace("ő", "o");

  str.replace("œ", "oe");

  str.replace("ŕ", "r");

  str.replace("ś", "s");
  str.replace("š", "s");
  str.replace("ș", "s");
  str.replace("ŝ", "s");

  str.replace("ß", "ss");

  str.replace("ť", "t");
  str.replace("ț", "t");

  str.replace("ú", "u");
  str.replace("ù", "u");
  str.replace("û", "u");
  str.replace("ü", "u");
  str.replace("ū", "u");
  str.replace("ů", "u");
  str.replace("ű", "u");

  str.replace("ŵ", "w");

  str.replace("ý", "y");
  str.replace("ÿ", "y");
  str.replace("ŷ", "y");

  str.replace("ž", "z");
  str.replace("ź", "z");
  str.replace("ż", "z");

  str.toUpperCase();

  String result = "";
  for (unsigned int i = 0; i < str.length(); i++)
  {
    char c = str.charAt(i);
    if ((c >= 'A' && c <= 'Z') || c == ' ')
    {
      result += c;
    }
  }
  return result;
}

bool isNumber(const char *str)
{
  for (int i = 0; str[i]; i++)
  {
    if (!isdigit(str[i]) && str[i] != '.' && str[i] != '-')
      return false;
  }
  return true;
}

bool isFiveDigitZip(const char *str)
{
  if (strlen(str) != 5)
    return false;
  for (int i = 0; i < 5; i++)
  {
    if (!isdigit(str[i]))
      return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// Weather Fetching and API settings
// -----------------------------------------------------------------------------
String buildWeatherURL()
{
  String base = "http://api.openweathermap.org/data/2.5/weather?";

  float lat = atof(openWeatherCity);
  float lon = atof(openWeatherCountry);

  bool latValid = isNumber(openWeatherCity) && isNumber(openWeatherCountry) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;

  if (latValid)
  {
    base += "lat=" + String(lat, 8) + "&lon=" + String(lon, 8);
  }
  else if (isFiveDigitZip(openWeatherCity) && String(openWeatherCountry).equalsIgnoreCase("US"))
  {
    base += "zip=" + String(openWeatherCity) + "," + String(openWeatherCountry);
  }
  else
  {
    base += "q=" + String(openWeatherCity) + "," + String(openWeatherCountry);
  }

  base += "&appid=" + String(openWeatherApiKey);
  base += "&units=" + String(weatherUnits);

  String langForAPI = String(language);

  if (langForAPI == "eo" || langForAPI == "ga" || langForAPI == "sw" || langForAPI == "ja")
  {
    langForAPI = "en";
  }
  base += "&lang=" + langForAPI;

  return base;
}

void fetchWeather()
{
  DEBUG_PRINTLN(F("[WEATHER] Fetching weather data..."));
  if (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINTLN(F("[WEATHER] Skipped: WiFi not connected"));
    weatherAvailable = false;
    weatherFetched = false;
    return;
  }
  if (!openWeatherApiKey || strlen(openWeatherApiKey) != 32)
  {
    DEBUG_PRINTLN(F("[WEATHER] Skipped: Invalid API key (must be exactly 32 characters)"));
    weatherAvailable = false;
    weatherFetched = false;
    return;
  }
  if (!(strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0))
  {
    DEBUG_PRINTLN(F("[WEATHER] Skipped: City or Country is empty."));
    weatherAvailable = false;
    return;
  }

  DEBUG_PRINTLN(F("[WEATHER] Connecting to OpenWeatherMap..."));
  String url = buildWeatherURL();
  DEBUG_PRINT(F("[WEATHER] URL: ")); // Use F() with Serial.print
  DEBUG_PRINTLN(url);

  HTTPClient http;   // Create an HTTPClient object
  WiFiClient client; // Create a WiFiClient object

  http.begin(client, url); // Pass the WiFiClient object and the URL

  http.setTimeout(10000); // Sets both connection and stream timeout to 10 seconds

  DEBUG_PRINTLN(F("[WEATHER] Sending GET request..."));
  int httpCode = http.GET(); // Send the GET request

  if (httpCode == HTTP_CODE_OK)
  { // Check if HTTP response code is 200 (OK)
    DEBUG_PRINTLN(F("[WEATHER] HTTP 200 OK. Reading payload..."));

    String payload = http.getString();
    DEBUG_PRINTLN(F("[WEATHER] Response received."));
    DEBUG_PRINT(F("[WEATHER] Payload: ")); // Use F() with Serial.print
    DEBUG_PRINTLN(payload);

    DynamicJsonDocument doc(1536); // Adjust size as needed, use ArduinoJson Assistant
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
      DEBUG_PRINT(F("[WEATHER] JSON parse error: "));
      DEBUG_PRINTLN(error.f_str());
      weatherAvailable = false;
      return;
    }

    if (doc.containsKey(F("main")) && doc[F("main")].containsKey(F("temp")))
    {
      float temp = doc[F("main")][F("temp")];
      currentTemp = String((int)round(temp)) + "º";
      DEBUG_PRINTF("[WEATHER] Temp: %s\n", currentTemp.c_str());
      weatherAvailable = true;
    }
    else
    {
      DEBUG_PRINTLN(F("[WEATHER] Temperature not found in JSON payload"));
      weatherAvailable = false;
      return;
    }

    if (doc.containsKey(F("main")) && doc[F("main")].containsKey(F("humidity")))
    {
      currentHumidity = doc[F("main")][F("humidity")];
      DEBUG_PRINTF("[WEATHER] Humidity: %d%%\n", currentHumidity);
    }
    else
    {
      currentHumidity = -1;
    }

    if (doc.containsKey(F("weather")) && doc[F("weather")].is<JsonArray>())
    {
      JsonObject weatherObj = doc[F("weather")][0];
      if (weatherObj.containsKey(F("main")))
      {
        mainDesc = weatherObj[F("main")].as<String>();
      }
      if (weatherObj.containsKey(F("description")))
      {
        detailedDesc = weatherObj[F("description")].as<String>();
      }
    }
    else
    {
      DEBUG_PRINTLN(F("[WEATHER] Weather description not found in JSON payload"));
    }

    weatherDescription = normalizeWeatherDescription(detailedDesc);
    DEBUG_PRINTF("[WEATHER] Description used: %s\n", weatherDescription.c_str());
    weatherFetched = true;
  }
  else
  {
    DEBUG_PRINTF("[WEATHER] HTTP GET failed, error code: %d, reason: %s\n", httpCode, http.errorToString(httpCode).c_str());
    weatherAvailable = false;
    weatherFetched = false;
  }

  http.end();
}

// -----------------------------------------------------------------------------
// Main setup() and loop()
// -----------------------------------------------------------------------------
/*
DisplayMode key:
  0: Clock
  1: Weather
  2: Weather Description
  3: Countdown (NEW)
*/

void setup()
{
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  delay(1000);
#endif
  DEBUG_PRINTLN();
  DEBUG_PRINTLN(F("[SETUP] Starting setup..."));

  if (!LittleFS.begin(true))
  {
    DEBUG_PRINTLN(F("[ERROR] LittleFS mount failed in setup! Halting."));
    while (true)
    {
      delay(1000);
      yield();
    }
  }
  DEBUG_PRINTLN(F("[SETUP] LittleFS file system mounted successfully."));

  DEBUG_PRINTLN(F("[SETUP] Initializing Parola library..."));
  P.begin(); // Initialize Parola library
  DEBUG_PRINTLN(F("[SETUP] Parola library initialized."));

  DEBUG_PRINTLN(F("[SETUP] Setting character spacing..."));
  P.setCharSpacing(0);
  DEBUG_PRINTLN(F("[SETUP] Character spacing set."));

  DEBUG_PRINTLN(F("[SETUP] Setting font..."));
  P.setFont(mFactory);
  DEBUG_PRINTLN(F("[SETUP] Font set."));

  loadConfig(); // This function now has internal yields and prints

  P.setIntensity(brightness);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);

  DEBUG_PRINTLN(F("[SETUP] Parola (LED Matrix) initialized"));

  connectWiFi();

  if (isAPMode)
  {
    DEBUG_PRINTLN(F("[SETUP] WiFi connection failed. Device is in AP Mode."));
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    DEBUG_PRINTLN(F("[SETUP] WiFi connected successfully to local network."));
  }
  else
  {
    DEBUG_PRINTLN(F("[SETUP] WiFi state is uncertain after connection attempt."));
  }

  setupWebServer();
  DEBUG_PRINTLN(F("[SETUP] Webserver setup complete"));
  DEBUG_PRINTLN(F("[SETUP] Setup complete"));
  DEBUG_PRINTLN();
  printConfigToSerial();
  setupTime();
  displayMode = 0;
  lastSwitch = millis();
  lastColonBlink = millis();
}

// Effect 1: Plasma - Dual rotating waves with radial interference
void renderPlasma(MD_MAX72XX* mx, uint16_t t) {
  for (uint8_t y = 0; y < 8; y++) {
    int8_t sy = y - 4;
    for (uint8_t x = 0; x < 32; x++) {
      int8_t sx = x - 16;
      uint8_t dist = (sx*sx + sy*sy);
      uint8_t v1 = ((x * 5 + t) ^ (y * 7 - t)) & 0xFF;
      uint8_t v2 = ((x * 3 - t*2) ^ (y * 9 + t)) & 0xFF;
      uint8_t v3 = (dist * 8 + t * 3) & 0xFF;
      uint8_t v = (v1 ^ v2) + v3;
      uint8_t threshold = 128 + ((t & 127) - 64);
      mx->setPoint(y, x, v > threshold);
    }
  }
}

// Effect 2: Tunnel - 3D tunnel diving effect
void renderTunnel(MD_MAX72XX* mx, uint16_t t) {
  for (uint8_t y = 0; y < 8; y++) {
    int8_t sy = (y - 4) * 8;
    for (uint8_t x = 0; x < 32; x++) {
      int8_t sx = (x - 16) * 2;
      uint16_t dist = sx*sx + sy*sy;
      if (dist == 0) dist = 1;
      uint8_t depth = 2048 / dist;
      uint8_t angle = (x * 8 + y * 32) & 0xFF;
      uint8_t v = (depth + t * 2) ^ (angle - t);
      mx->setPoint(y, x, v & 32);
    }
  }
}

// Effect 3: Matrix Rain - Cascading columns
void renderMatrixRain(MD_MAX72XX* mx, uint16_t t) {
  static uint8_t drops[32];
  static uint8_t speeds[32];
  static bool init = false;

  if (!init || (t & 0x1F) == 0) {
    for (uint8_t x = 0; x < 32; x++) {
      drops[x] = (x * 7 + t) & 31;
      speeds[x] = 1 + ((x * 3) & 3);
    }
    init = true;
  }

  for (uint8_t x = 0; x < 32; x++) {
    drops[x] = (drops[x] + speeds[x]) & 31;
    uint8_t head = drops[x] >> 2;
    for (uint8_t y = 0; y < 8; y++) {
      uint8_t trail = (head + 8 - y) & 7;
      bool lit = (trail < 3) && ((x + y + t) & 1);
      mx->setPoint(y, x, lit);
    }
  }
}

// Effect 4: Starfield - 3D scrolling stars
void renderStarfield(MD_MAX72XX* mx, uint16_t t) {
  static uint16_t stars[24];
  static bool init = false;

  if (!init) {
    for (uint8_t i = 0; i < 24; i++) {
      stars[i] = ((i * 127) << 8) | ((i * 83) & 0xFF);
    }
    init = true;
  }

  mx->clear();
  for (uint8_t i = 0; i < 24; i++) {
    uint8_t z = (stars[i] & 0xFF) + (t >> 2);
    stars[i] = (stars[i] & 0xFF00) | z;

    if (z < 32) {
      int16_t sx = (stars[i] >> 8);
      int16_t sy = ((sx * 3) & 0xFF);
      sx = ((sx - 128) * (32 - z)) >> 8;
      sy = ((sy - 128) * (32 - z)) >> 8;
      int8_t px = 16 + sx;
      int8_t py = 4 + (sy >> 4);

      if (px >= 0 && px < 32 && py >= 0 && py < 8) {
        mx->setPoint(py, px, true);
        if (z < 8 && px > 0) mx->setPoint(py, px-1, true);
      }
    }
  }
}

// Effect 5: Fire - Rising flames simulation
void renderFire(MD_MAX72XX* mx, uint16_t t) {
  static uint8_t heat[32];

  // Add heat at bottom
  for (uint8_t x = 0; x < 32; x++) {
    if ((t + x) & 3) {
      heat[x] = 255 - ((x ^ t) & 31);
    }
  }

  // Render with rising and spreading
  for (int8_t y = 7; y >= 0; y--) {
    for (uint8_t x = 0; x < 32; x++) {
      uint8_t h = heat[x];

      // Cool down as it rises
      if (y < 7) h = (h * (7 - y)) >> 3;

      // Spread from neighbors
      if (x > 0) h = (h + heat[x-1]) >> 1;
      if (x < 31) h = (h + heat[x+1]) >> 1;

      mx->setPoint(y, x, h > (128 + ((y * 16) ^ t)));
    }
  }

  // Cool down base
  for (uint8_t x = 0; x < 32; x++) {
    heat[x] = (heat[x] * 230) >> 8;
  }
}

void renderPlasmaEffect(unsigned long frameTime)
{
  static uint8_t effectIndex = 0;
  static unsigned long lastEffectChange = 0;
  const unsigned long EFFECT_DURATION = 2000; // 2 seconds per effect

  MD_MAX72XX* mx = P.getGraphicObject();
  uint16_t t = frameTime >> 5;

  // Switch effects every 2 seconds
  if (frameTime - lastEffectChange > EFFECT_DURATION) {
    lastEffectChange = frameTime;
    effectIndex = (effectIndex + 1) % 5;
    mx->clear();
  }

  switch(effectIndex) {
    case 0: renderPlasma(mx, t); break;
    case 1: renderTunnel(mx, t); break;
    case 2: renderMatrixRain(mx, t); break;
    case 3: renderStarfield(mx, t); break;
    case 4: renderFire(mx, t); break;
  }
}

void advanceDisplayMode()
{
  int oldMode = displayMode;
  String ntpField = String(ntpServer2);
  bool nightscoutConfigured = ntpField.startsWith("https://");

  if (displayMode == 0)
  { // Clock
    if (showDate)
    {
      displayMode = 5; // Date mode right after Clock
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DATE (from Clock)"));
    }
    else if (weatherAvailable && (strlen(openWeatherApiKey) == 32) && (strlen(openWeatherCity) > 0) && (strlen(openWeatherCountry) > 0))
    {
      displayMode = 1;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: WEATHER (from Clock)"));
    }
    else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful)
    {
      displayMode = 3;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Clock, weather skipped)"));
    }
    else if (nightscoutConfigured)
    {
      displayMode = 4; // Clock -> Nightscout (if weather & countdown are skipped)
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Clock, weather & countdown skipped)"));
    }
    else
    {
      displayMode = 6; // Clock -> Demoscene (if everything else is skipped)
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Clock)"));
    }
  }
  else if (displayMode == 5)
  { // Date mode
    if (weatherAvailable && (strlen(openWeatherApiKey) == 32) && (strlen(openWeatherCity) > 0) && (strlen(openWeatherCountry) > 0))
    {
      displayMode = 1;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: WEATHER (from Date)"));
    }
    else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful)
    {
      displayMode = 3;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Date, weather skipped)"));
    }
    else if (nightscoutConfigured)
    {
      displayMode = 4;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Date, weather & countdown skipped)"));
    }
    else
    {
      displayMode = 6;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Date)"));
    }
  }
  else if (displayMode == 1)
  { // Weather
    if (showWeatherDescription && weatherAvailable && weatherDescription.length() > 0)
    {
      displayMode = 2;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DESCRIPTION (from Weather)"));
    }
    else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful)
    {
      displayMode = 3;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Weather)"));
    }
    else if (nightscoutConfigured)
    {
      displayMode = 4; // Weather -> Nightscout (if description & countdown are skipped)
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Weather, description & countdown skipped)"));
    }
    else
    {
      displayMode = 6;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Weather)"));
    }
  }
  else if (displayMode == 2)
  { // Weather Description
    if (countdownEnabled && !countdownFinished && ntpSyncSuccessful)
    {
      displayMode = 3;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Description)"));
    }
    else if (nightscoutConfigured)
    {
      displayMode = 4; // Description -> Nightscout (if countdown is skipped)
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Description, countdown skipped)"));
    }
    else
    {
      displayMode = 6;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Description)"));
    }
  }
  else if (displayMode == 3)
  { // Countdown -> Nightscout or Demoscene
    if (nightscoutConfigured)
    {
      displayMode = 4;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Countdown)"));
    }
    else
    {
      displayMode = 6;
      DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Countdown)"));
    }
  }
  else if (displayMode == 4)
  { // Nightscout -> Demoscene
    displayMode = 6;
    DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: DEMOSCENE (from Nightscout)"));
  }
  else if (displayMode == 6)
  { // Demoscene -> Clock
    displayMode = 0;
    DEBUG_PRINTLN(F("[DISPLAY] Switching to display mode: CLOCK (from Demoscene)"));
  }

  // --- Common cleanup/reset logic remains the same ---
  lastSwitch = millis();
}

// config save after countdown finishes
bool saveCountdownConfig(bool enabled, time_t targetTimestamp, const String &label)
{
  DynamicJsonDocument doc(2048);

  File configFile = LittleFS.open("/config.json", "r");
  if (configFile)
  {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err)
    {
      DEBUG_PRINT(F("[saveCountdownConfig] Error parsing config.json: "));
      DEBUG_PRINTLN(err.f_str());
      return false;
    }
  }

  JsonObject countdownObj = doc["countdown"].is<JsonObject>() ? doc["countdown"].as<JsonObject>() : doc.createNestedObject("countdown");
  countdownObj["enabled"] = enabled;
  countdownObj["targetTimestamp"] = targetTimestamp;
  countdownObj["label"] = label;
  countdownObj["isDramaticCountdown"] = isDramaticCountdown;
  doc.remove("countdownEnabled");
  doc.remove("countdownDate");
  doc.remove("countdownTime");
  doc.remove("countdownLabel");

  if (LittleFS.exists("/config.json"))
  {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f)
  {
    DEBUG_PRINTLN(F("[saveCountdownConfig] ERROR: Cannot write to /config.json"));
    return false;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();

  DEBUG_PRINTF("[saveCountdownConfig] Config updated. %u bytes written.\n", bytesWritten);
  return true;
}

void loop()
{
  if (isAPMode)
  {
    dnsServer.processNextRequest();
  }

  static bool colonVisible = true;
  const unsigned long colonBlinkInterval = 800;
  if (millis() - lastColonBlink > colonBlinkInterval)
  {
    colonVisible = !colonVisible;
    lastColonBlink = millis();
  }

  static unsigned long ntpAnimTimer = 0;
  static int ntpAnimFrame = 0;
  static bool tzSetAfterSync = false;

  static unsigned long lastFetch = 0;
  const unsigned long fetchInterval = 300000; // 5 minutes

  // AP Mode animation
  static unsigned long apAnimTimer = 0;
  static int apAnimFrame = 0;
  if (isAPMode)
  {
    unsigned long now = millis();
    if (now - apAnimTimer > 750)
    {
      apAnimTimer = now;
      apAnimFrame++;
    }
    P.setTextAlignment(PA_CENTER);
    switch (apAnimFrame % 3)
    {
    case 0:
      P.print(F("= ©"));
      break;
    case 1:
      P.print(F("= ª"));
      break;
    case 2:
      P.print(F("= «"));
      break;
    }
    yield();
    return;
  }

  // Dimming
  time_t now_time = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now_time, &timeinfo);
  int curHour = timeinfo.tm_hour;
  int curMinute = timeinfo.tm_min;
  int curTotal = curHour * 60 + curMinute;
  int startTotal = dimStartHour * 60 + dimStartMinute;
  int endTotal = dimEndHour * 60 + dimEndMinute;
  bool isDimmingActive = false;

  if (dimmingEnabled)
  {
    if (startTotal < endTotal)
    {
      isDimmingActive = (curTotal >= startTotal && curTotal < endTotal);
    }
    else
    { // Overnight dimming
      isDimmingActive = (curTotal >= startTotal || curTotal < endTotal);
    }
    if (isDimmingActive)
    {
      P.setIntensity(dimBrightness);
    }
    else
    {
      P.setIntensity(brightness);
    }
  }
  else
  {
    P.setIntensity(brightness);
  }

  // --- IMMEDIATE COUNTDOWN FINISH TRIGGER ---
  if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && now_time >= countdownTargetTimestamp)
  {
    countdownFinished = true;
    displayMode = 3; // Let main loop handle animation + TIMES UP
    countdownShowFinishedMessage = true;
    hourglassPlayed = false;
    countdownFinishedMessageStartTime = millis();

    DEBUG_PRINTLN("[SYSTEM] Countdown target reached! Switching to Mode 3 to display finish sequence.");
    yield();
  }

  // --- IP Display ---
  if (showingIp)
  {
    if (P.displayAnimate())
    {
      ipDisplayCount++;
      if (ipDisplayCount < ipDisplayMax)
      {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, scrollSpeed);
      }
      else
      {
        showingIp = false;
        P.displayClear();
        delay(500); // Blocking delay as in working copy
        displayMode = 0;
        lastSwitch = millis();
      }
    }
    yield();
    return; // Exit loop early if showing IP
  }

  // --- NTP State Machine ---
  switch (ntpState)
  {
  case NTP_IDLE:
    break;
  case NTP_SYNCING:
  {
    time_t now = time(nullptr);
    if (now > 1000)
    { // NTP sync successful
      DEBUG_PRINTLN(F("[TIME] NTP sync successful."));
      ntpSyncSuccessful = true;
      ntpState = NTP_SUCCESS;
    }
    else if (millis() - ntpStartTime > ntpTimeout || ntpRetryCount >= maxNtpRetries)
    {
      DEBUG_PRINTLN(F("[TIME] NTP sync failed."));
      ntpSyncSuccessful = false;
      ntpState = NTP_FAILED;
    }
    else
    {
      // Periodically print a more descriptive status message
      if (millis() - lastNtpStatusPrintTime >= ntpStatusPrintInterval)
      {
        DEBUG_PRINTF("[TIME] NTP sync in progress (attempt %d of %d)...\n", ntpRetryCount + 1, maxNtpRetries);
        lastNtpStatusPrintTime = millis();
      }
      // Still increment ntpRetryCount based on your original timing for the timeout logic
      // (even if you don't print a dot for every increment)
      if (millis() - ntpStartTime > ((unsigned long)(ntpRetryCount + 1) * 1000UL))
      {
        ntpRetryCount++;
      }
    }
    break;
  }
  case NTP_SUCCESS:
    if (!tzSetAfterSync)
    {
      const char *posixTz = ianaToPosix(timeZone);
      setenv("TZ", posixTz, 1);
      tzset();
      tzSetAfterSync = true;
    }
    ntpAnimTimer = 0;
    ntpAnimFrame = 0;
    break;

  case NTP_FAILED:
    ntpAnimTimer = 0;
    ntpAnimFrame = 0;

    static unsigned long lastNtpRetryAttempt = 0;
    static bool firstRetry = true;

    if (lastNtpRetryAttempt == 0)
    {
      lastNtpRetryAttempt = millis(); // set baseline on first fail
    }

    unsigned long ntpRetryInterval = firstRetry ? 30000UL : 300000UL; // first retry after 30s, after that every 5 minutes

    if (millis() - lastNtpRetryAttempt > ntpRetryInterval)
    {
      lastNtpRetryAttempt = millis();
      ntpRetryCount = 0;
      ntpStartTime = millis();
      ntpState = NTP_SYNCING;
      DEBUG_PRINTLN(F("[TIME] Retrying NTP sync..."));

      firstRetry = false;
    }
    break;
  }

  // Only advance mode by timer for clock/weather, not description!
  unsigned long displayDuration = (displayMode == 0) ? clockDuration : weatherDuration;
  if ((displayMode == 0 || displayMode == 1) && millis() - lastSwitch > displayDuration)
  {
    advanceDisplayMode();
  }

  // --- MODIFIED WEATHER FETCHING LOGIC ---
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!weatherFetchInitiated || shouldFetchWeatherNow || (millis() - lastFetch > fetchInterval))
    {
      if (shouldFetchWeatherNow)
      {
        DEBUG_PRINTLN(F("[LOOP] Immediate weather fetch requested by web server."));
        shouldFetchWeatherNow = false;
      }
      else if (!weatherFetchInitiated)
      {
        DEBUG_PRINTLN(F("[LOOP] Initial weather fetch."));
      }
      else
      {
        DEBUG_PRINTLN(F("[LOOP] Regular interval weather fetch."));
      }
      weatherFetchInitiated = true;
      weatherFetched = false;
      fetchWeather();
      lastFetch = millis();
    }
  }
  else
  {
    weatherFetchInitiated = false;
    shouldFetchWeatherNow = false;
  }

  const char *const *daysOfTheWeek = getDaysOfWeek(language);
  const char *daySymbol = daysOfTheWeek[timeinfo.tm_wday];

  // build base HH:MM first ---
  char baseTime[9];
  if (twelveHourToggle)
  {
    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0)
      hour12 = 12;
    sprintf(baseTime, "%d:%02d", hour12, timeinfo.tm_min);
  }
  else
  {
    sprintf(baseTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }

  // add seconds only if colon blink enabled AND weekday hidden ---
  char timeWithSeconds[12];
  if (!showDayOfWeek && colonBlinkEnabled)
  {
    // Remove any leading space from baseTime
    const char *trimmedBase = baseTime;
    if (baseTime[0] == ' ')
      trimmedBase++; // skip leading space
    sprintf(timeWithSeconds, "%s:%02d", trimmedBase, timeinfo.tm_sec);
  }
  else
  {
    strcpy(timeWithSeconds, baseTime); // no seconds
  }

  // keep spacing logic the same ---
  char timeSpacedStr[24];
  int j = 0;
  for (int i = 0; timeWithSeconds[i] != '\0'; i++)
  {
    timeSpacedStr[j++] = timeWithSeconds[i];
    if (timeWithSeconds[i + 1] != '\0')
    {
      timeSpacedStr[j++] = ' ';
    }
  }
  timeSpacedStr[j] = '\0';

  // build final string ---
  String formattedTime;
  if (showDayOfWeek)
  {
    formattedTime = String(daySymbol) + "   " + String(timeSpacedStr);
  }
  else
  {
    formattedTime = String(timeSpacedStr);
  }

  unsigned long currentDisplayDuration = 0;
  if (displayMode == 0)
  {
    currentDisplayDuration = clockDuration;
  }
  else if (displayMode == 1)
  { // Weather
    currentDisplayDuration = weatherDuration;
  }

  // Only advance mode by timer for clock/weather static (Mode 0 & 1).
  // Other modes (2, 3) have their own internal timers/conditions for advancement.
  if ((displayMode == 0 || displayMode == 1) && (millis() - lastSwitch > currentDisplayDuration))
  {
    advanceDisplayMode();
  }

  // Persistent variables (declare near top of file or loop)
  static int prevDisplayMode = -1;
  static bool clockScrollDone = false;

  // --- CLOCK Display Mode ---
  if (displayMode == 0)
  {
    P.setCharSpacing(0);

    // --- NTP SYNC ---
    if (ntpState == NTP_SYNCING)
    {
      if (ntpSyncSuccessful || ntpRetryCount >= maxNtpRetries || millis() - ntpStartTime > ntpTimeout)
      {
        ntpState = NTP_FAILED;
      }
      else if (millis() - ntpAnimTimer > 750)
      {
        ntpAnimTimer = millis();
        switch (ntpAnimFrame % 3)
        {
        case 0:
          P.print(F("S Y N C ®"));
          break;
        case 1:
          P.print(F("S Y N C ¯"));
          break;
        case 2:
          P.print(F("S Y N C °"));
          break;
        }
        ntpAnimFrame++;
      }
    }
    // --- NTP / WEATHER ERROR ---
    else if (!ntpSyncSuccessful)
    {
      P.setTextAlignment(PA_CENTER);
      static unsigned long errorAltTimer = 0;
      static bool showNtpError = true;

      if (!ntpSyncSuccessful && !weatherAvailable)
      {
        if (millis() - errorAltTimer > 2000)
        {
          errorAltTimer = millis();
          showNtpError = !showNtpError;
        }
        P.print(showNtpError ? F("?/") : F("?*"));
      }
      else if (!ntpSyncSuccessful)
      {
        P.print(F("?/"));
      }
      else if (!weatherAvailable)
      {
        P.print(F("?*"));
      }
    }
    // --- DISPLAY CLOCK ---
    else
    {
      String timeString = formattedTime;
      if (showDayOfWeek && colonBlinkEnabled && !colonVisible)
      {
        timeString.replace(":", " ");
      }

      // --- SCROLL IN ONLY WHEN COMING FROM SPECIFIC MODES OR FIRST BOOT ---
      bool shouldScrollIn = false;
      if (prevDisplayMode == -1 || prevDisplayMode == 3 || prevDisplayMode == 4)
      {
        shouldScrollIn = true; // first boot or other special modes
      }
      else if (prevDisplayMode == 2 && weatherDescription.length() > 8)
      {
        shouldScrollIn = true; // only scroll in if weather was scrolling
      }

      if (shouldScrollIn && !clockScrollDone)
      {
        textEffect_t inDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);

        P.displayText(
            timeString.c_str(),
            PA_CENTER,
            scrollSpeed,
            0,
            inDir,
            PA_NO_EFFECT);
        while (!P.displayAnimate())
          yield();
        clockScrollDone = true; // mark scroll done
      }
      else
      {
        P.setTextAlignment(PA_CENTER);
        P.print(timeString);
      }
    }

    yield();
  }
  else
  {
    // --- leaving clock mode ---
    if (prevDisplayMode == 0)
    {
      clockScrollDone = false; // reset for next time we enter clock
    }
  }

  // --- update prevDisplayMode ---
  prevDisplayMode = displayMode;

  // --- WEATHER Display Mode ---
  static bool weatherWasAvailable = false;
  if (displayMode == 1)
  {
    P.setCharSpacing(1);
    if (weatherAvailable)
    {
      String weatherDisplay;
      if (showHumidity && currentHumidity != -1)
      {
        int cappedHumidity = (currentHumidity > 99) ? 99 : currentHumidity;
        weatherDisplay = currentTemp + " " + String(cappedHumidity) + "%";
      }
      else
      {
        weatherDisplay = currentTemp + tempSymbol;
      }
      P.print(weatherDisplay.c_str());
      weatherWasAvailable = true;
    }
    else
    {
      if (weatherWasAvailable)
      {
        DEBUG_PRINTLN(F("[DISPLAY] Weather not available, showing clock..."));
        weatherWasAvailable = false;
      }
      if (ntpSyncSuccessful)
      {
        String timeString = formattedTime;
        if (!colonVisible)
          timeString.replace(":", " ");
        P.setCharSpacing(0);
        P.print(timeString);
      }
      else
      {
        P.setCharSpacing(0);
        P.setTextAlignment(PA_CENTER);
        P.print(F("?*"));
      }
    }
    yield();
    return;
  }

  // --- WEATHER DESCRIPTION Display Mode ---
  if (displayMode == 2 && showWeatherDescription && weatherAvailable && weatherDescription.length() > 0)
  {
    String desc = weatherDescription;

    // prepare safe buffer
    static char descBuffer[128]; // large enough for OWM translations
    desc.toCharArray(descBuffer, sizeof(descBuffer));

    if (desc.length() > 8)
    {
      if (!descScrolling)
      {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(descBuffer, PA_CENTER, actualScrollDirection, scrollSpeed);
        descScrolling = true;
        descScrollEndTime = 0; // reset end time at start
      }
      if (P.displayAnimate())
      {
        if (descScrollEndTime == 0)
        {
          descScrollEndTime = millis(); // mark the time when scroll finishes
        }
        // wait small pause after scroll stops
        if (millis() - descScrollEndTime > descriptionScrollPause)
        {
          descScrolling = false;
          descScrollEndTime = 0;
          advanceDisplayMode();
        }
      }
      else
      {
        descScrollEndTime = 0; // reset if not finished
      }
      yield();
      return;
    }
    else
    {
      if (descStartTime == 0)
      {
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(1);
        P.print(descBuffer);
        descStartTime = millis();
      }
      if (millis() - descStartTime > descriptionDuration)
      {
        descStartTime = 0;
        advanceDisplayMode();
      }
      yield();
      return;
    }
  }

  // --- Countdown Display Mode ---
  if (displayMode == 3 && countdownEnabled && ntpSyncSuccessful)
  {
    static int countdownSegment = 0;
    static unsigned long segmentStartTime = 0;
    const unsigned long SEGMENT_DISPLAY_DURATION = 1500; // 1.5 seconds for each static segment

    long timeRemaining = countdownTargetTimestamp - now_time;

    // --- Countdown Finished Logic ---
    // This part of the code remains unchanged.
    if (timeRemaining <= 0 || countdownShowFinishedMessage)
    {
      // NEW: Only show "TIMES UP" if countdown target timestamp is valid and expired
      time_t now = time(nullptr);
      if (countdownTargetTimestamp == 0 || countdownTargetTimestamp > now)
      {
        // Target invalid or in the future, don't show "TIMES UP" yet, advance display instead
        countdownShowFinishedMessage = false;
        countdownFinished = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false; // Reset if we decide not to show it
        DEBUG_PRINTLN("[COUNTDOWN-FINISH] Countdown target invalid or not reached yet, skipping 'TIMES UP'. Advancing display.");
        advanceDisplayMode();
        yield();
        return;
      }

      // Define these static variables here if they are not global (or already defined in your loop())
      static const char *flashFrames[] = {"{|", "}~"};
      static unsigned long lastFlashingSwitch = 0;
      static int flashingMessageFrame = 0;

      // --- Initial Combined Sequence: Play Hourglass THEN start Flashing ---
      // This 'if' runs ONLY ONCE when the "finished" sequence begins.
      if (!hourglassPlayed)
      {                                               // <-- This is the single entry point for the combined sequence
        countdownFinished = true;                     // Mark as finished overall
        countdownShowFinishedMessage = true;          // Confirm we are in the finished sequence
        countdownFinishedMessageStartTime = millis(); // Start the 15-second timer for the flashing duration

        // 1. Play Hourglass Animation (Blocking)
        const char *hourglassFrames[] = {"¡", "¢", "£", "¤"};
        for (int repeat = 0; repeat < 3; repeat++)
        {
          for (int i = 0; i < 4; i++)
          {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(0);
            P.print(hourglassFrames[i]);
            delay(350); // This is blocking! (Total ~4.2 seconds for hourglass)
          }
        }
        DEBUG_PRINTLN("[COUNTDOWN-FINISH] Played hourglass animation.");
        P.displayClear(); // Clear display after hourglass animation

        // 2. Initialize Flashing "TIMES UP" for its very first frame
        flashingMessageFrame = 0;
        lastFlashingSwitch = millis(); // Set initial time for first flash frame
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(0);
        P.print(flashFrames[flashingMessageFrame]);            // Display the first frame immediately
        flashingMessageFrame = (flashingMessageFrame + 1) % 2; // Prepare for the next frame

        hourglassPlayed = true; // <-- Mark that this initial combined sequence has completed!
        countdownSegment = 0;   // Reset segment counter after finished sequence initiation
        segmentStartTime = 0;   // Reset segment timer after finished sequence initiation
      }

      // --- Continue Flashing "TIMES UP" for its duration (after initial combined sequence) ---
      // This part runs in subsequent loop iterations after the hourglass has played.
      if (millis() - countdownFinishedMessageStartTime < 15000)
      { // Flashing duration
        if (millis() - lastFlashingSwitch >= 500)
        { // Check for flashing interval
          lastFlashingSwitch = millis();
          P.displayClear();
          P.setTextAlignment(PA_CENTER);
          P.setCharSpacing(0);
          P.print(flashFrames[flashingMessageFrame]);
          flashingMessageFrame = (flashingMessageFrame + 1) % 2;
        }
        P.displayAnimate(); // Ensure display updates
        yield();
        return; // Stay in this mode until the 15 seconds are over
      }
      else
      {
        // 15 seconds are over, clean up and advance
        DEBUG_PRINTLN("[COUNTDOWN-FINISH] Flashing duration over. Advancing to Clock.");
        countdownShowFinishedMessage = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false; // <-- RESET this flag for the next countdown cycle!

        // Final cleanup (persisted)
        countdownEnabled = false;
        countdownTargetTimestamp = 0;
        countdownLabel[0] = '\0';
        saveCountdownConfig(false, 0, "");

        P.setInvert(false);
        advanceDisplayMode();
        yield();
        return; // Exit loop after processing
      }
    } // END of 'if (timeRemaining <= 0 || countdownShowFinishedMessage)'

    // --- NORMAL COUNTDOWN LOGIC ---
    // This 'else' block will only run if `timeRemaining > 0` and `!countdownShowFinishedMessage`
    else
    {

      // The new variable `isDramaticCountdown` toggles between the two modes
      if (isDramaticCountdown)
      {
        // --- EXISTING DRAMATIC COUNTDOWN LOGIC ---
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;
        String currentSegmentText = "";

        if (segmentStartTime == 0 || (millis() - segmentStartTime > SEGMENT_DISPLAY_DURATION))
        {
          segmentStartTime = millis();
          P.displayClear();

          switch (countdownSegment)
          {
          case 0: // Days
            if (days > 0)
            {
              currentSegmentText = String(days) + " " + (days == 1 ? "DAY" : "DAYS");
              DEBUG_PRINTF("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
              countdownSegment++;
            }
            else
            {
              // Skip days if zero
              countdownSegment++;
              segmentStartTime = 0;
            }
            break;
          case 1:
          { // Hours
            char buf[10];
            sprintf(buf, "%02ld HRS", hours); // pad hours with 0
            currentSegmentText = String(buf);
            DEBUG_PRINTF("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
            countdownSegment++;
            break;
          }
          case 2:
          { // Minutes
            char buf[10];
            sprintf(buf, "%02ld MINS", minutes); // pad minutes with 0
            currentSegmentText = String(buf);
            DEBUG_PRINTF("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
            countdownSegment++;
            break;
          }
          case 3:
          { // Seconds & Label Scroll
            time_t segmentStartTime = time(nullptr);
            unsigned long segmentStartMillis = millis();

            long nowRemaining = countdownTargetTimestamp - segmentStartTime;
            long currentSecond = nowRemaining % 60;
            char secondsBuf[10];
            sprintf(secondsBuf, "%02ld %s", currentSecond, currentSecond == 1 ? "SEC" : "SECS");
            String secondsText = String(secondsBuf);
            DEBUG_PRINTF("[COUNTDOWN-STATIC] Displaying segment 3: %s\n", secondsText.c_str());
            P.displayClear();
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            P.print(secondsText.c_str());
            delay(SEGMENT_DISPLAY_DURATION - 400);

            unsigned long elapsed = millis() - segmentStartMillis;
            long adjustedSecond = (countdownTargetTimestamp - segmentStartTime - (elapsed / 1000)) % 60;
            sprintf(secondsBuf, "%02ld %s", adjustedSecond, adjustedSecond == 1 ? "SEC" : "SECS");
            secondsText = String(secondsBuf);
            P.displayClear();
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            P.print(secondsText.c_str());
            delay(400);

            String label;
            if (strlen(countdownLabel) > 0)
            {
              label = String(countdownLabel);
              label.trim();
              if (!label.startsWith("TO:") && !label.startsWith("to:"))
              {
                label = "TO: " + label;
              }
              label.replace('.', ',');
            }
            else
            {
              static const char *fallbackLabels[] = {
                  "TO: PARTY TIME!", "TO: SHOWTIME!", "TO: CLOCKOUT!", "TO: BLASTOFF!",
                  "TO: GO TIME!", "TO: LIFTOFF!", "TO: THE BIG REVEAL!",
                  "TO: ZERO HOUR!", "TO: THE FINAL COUNT!", "TO: MISSION COMPLETE"};
              int randomIndex = random(0, 10);
              label = fallbackLabels[randomIndex];
            }

            P.setTextAlignment(PA_LEFT);
            P.setCharSpacing(1);
            textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
            P.displayScroll(label.c_str(), PA_LEFT, actualScrollDirection, scrollSpeed);

            while (!P.displayAnimate())
            {
              yield();
            }
            countdownSegment++;
            segmentStartTime = millis();
            break;
          }
          case 4: // Exit countdown
            DEBUG_PRINTLN("[COUNTDOWN-STATIC] All segments and label displayed. Advancing to Clock.");
            countdownSegment = 0;
            segmentStartTime = 0;
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            advanceDisplayMode();
            yield();
            return;

          default:
            DEBUG_PRINTLN("[COUNTDOWN-ERROR] Invalid countdownSegment, resetting.");
            countdownSegment = 0;
            segmentStartTime = 0;
            break;
          }

          if (currentSegmentText.length() > 0)
          {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            P.print(currentSegmentText.c_str());
          }
        }
        P.displayAnimate();
      }

      // --- NEW: SINGLE-LINE COUNTDOWN LOGIC ---
      else
      {
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;

        String label;
        // Check if countdownLabel is empty and grab a random one if needed
        if (strlen(countdownLabel) > 0)
        {
          label = String(countdownLabel);
          label.trim();
        }
        else
        {
          static const char *fallbackLabels[] = {
              "PARTY TIME", "SHOWTIME", "CLOCKOUT", "BLASTOFF",
              "GO TIME", "LIFTOFF", "THE BIG REVEAL",
              "ZERO HOUR", "THE FINAL COUNT", "MISSION COMPLETE"};
          int randomIndex = random(0, 10);
          label = fallbackLabels[randomIndex];
        }

        // Format the full string
        char buf[50];
        // Only show days if there are any, otherwise start with hours
        if (days > 0)
        {
          sprintf(buf, "%s IN: %ldD %02ldH %02ldM %02ldS", label.c_str(), days, hours, minutes, seconds);
        }
        else
        {
          sprintf(buf, "%s IN: %02ldH %02ldM %02ldS", label.c_str(), hours, minutes, seconds);
        }

        String fullString = String(buf);

        // Display the full string and scroll it
        P.setTextAlignment(PA_LEFT);
        P.setCharSpacing(1);
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(fullString.c_str(), PA_LEFT, actualScrollDirection, scrollSpeed);

        // Blocking loop to ensure the full message scrolls
        while (!P.displayAnimate())
        {
          yield();
        }

        // After scrolling is complete, we're done with this display mode
        // Move to the next mode and exit the function.
        P.setTextAlignment(PA_CENTER);
        advanceDisplayMode();
        yield();
        return;
      }
    }

    // Keep alignment reset just in case
    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(1);
    yield();
    return;
  } // End of if (displayMode == 3 && ...)

  // --- NIGHTSCOUT Display Mode ---
  if (displayMode == 4)
  {
    String ntpField = String(ntpServer2);

    // These static variables will retain their values between calls to this block
    static unsigned long lastNightscoutFetchTime = 0;
    const unsigned long NIGHTSCOUT_FETCH_INTERVAL = 150000; // 2.5 minutes
    static int currentGlucose = -1;
    static String currentDirection = "?";

    // Check if it's time to fetch new data or if we have no data yet
    if (currentGlucose == -1 || millis() - lastNightscoutFetchTime >= NIGHTSCOUT_FETCH_INTERVAL)
    {
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient https;
      https.begin(client, ntpField);
      https.setConnectTimeout(5000);
      https.setTimeout(5000);

      DEBUG_PRINT("[HTTPS] Nightscout fetch initiated...\n");
      int httpCode = https.GET();

      if (httpCode == HTTP_CODE_OK)
      {
        String payload = https.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.is<JsonArray>() && doc.size() > 0)
        {
          JsonObject firstReading = doc[0].as<JsonObject>();
          currentGlucose = firstReading["glucose"] | firstReading["sgv"] | -1;
          currentDirection = firstReading["direction"] | "?";

          DEBUG_PRINTF("Nightscout data fetched: mg/dL %d %s\n", currentGlucose, currentDirection.c_str());
        }
        else
        {
          DEBUG_PRINTLN("Failed to parse Nightscout JSON");
        }
      }
      else
      {
        DEBUG_PRINTF("[HTTPS] GET failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
      lastNightscoutFetchTime = millis(); // Update the timestamp
    }

    // Display the data we have, which is now stored in static variables
    if (currentGlucose != -1)
    {
      char arrow;
      if (currentDirection == "Flat")
        arrow = 139;
      else if (currentDirection == "SingleUp")
        arrow = 134;
      else if (currentDirection == "DoubleUp")
        arrow = 135;
      else if (currentDirection == "SingleDown")
        arrow = 136;
      else if (currentDirection == "DoubleDown")
        arrow = 137;
      else if (currentDirection == "FortyFiveUp")
        arrow = 138;
      else if (currentDirection == "FortyFiveDown")
        arrow = 140;
      else
        arrow = '?';

      String displayText = String(currentGlucose) + String(arrow);

      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(1);
      P.print(displayText.c_str());

      delay(weatherDuration);
      advanceDisplayMode();
      return;
    }
    else
    {
      // If no data is available after the first fetch attempt, show an error and advance
      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(0);
      P.print(F("?)"));
      delay(2000); // Wait 2 seconds before advancing
      advanceDisplayMode();
      return;
    }
  }

  // --- DEMOSCENE Display Mode ---
  else if (displayMode == 6)
  {
    static unsigned long demoStartTime = 0;

    // Initialize start time when entering this mode
    if (demoStartTime == 0 || prevDisplayMode != 6)
    {
      demoStartTime = millis();
      DEBUG_PRINTLN(F("[DISPLAY] Started Demoscene plasma effect"));
    }

    // Render plasma effect
    renderPlasmaEffect(millis() - demoStartTime);

    // Check if demo duration has elapsed
    if (millis() - demoStartTime >= demoDuration)
    {
      demoStartTime = 0; // Reset for next time
      DEBUG_PRINTLN(F("[DISPLAY] Demoscene effect completed, advancing mode"));
      advanceDisplayMode();
      return;
    }

    yield();
  }

  // DATE Display Mode
  else if (displayMode == 5 && showDate)
  {

    // --- VALID DATE CHECK ---
    if (timeinfo.tm_year < 120 || timeinfo.tm_mday <= 0 || timeinfo.tm_mon < 0 || timeinfo.tm_mon > 11)
    {
      advanceDisplayMode();
      return; // skip drawing
    }
    // -------------------------
    String dateString;

    // Get localized month names
    const char *const *months = getMonthsOfYear(language);
    String monthAbbr = String(months[timeinfo.tm_mon]).substring(0, 5);
    monthAbbr.toLowerCase();

    // Add spaces between day digits
    String dayString = String(timeinfo.tm_mday);
    String spacedDay = "";
    for (size_t i = 0; i < dayString.length(); i++)
    {
      spacedDay += dayString[i];
      if (i < dayString.length() - 1)
        spacedDay += " ";
    }

    // Function to check if day should come first for given language
    auto isDayFirst = [](const String &lang)
    {
      // Languages with DD-MM order
      const char *dayFirstLangs[] = {
          "af", // Afrikaans
          "cs", // Czech
          "da", // Danish
          "de", // German
          "eo", // Esperanto
          "es", // Spanish
          "et", // Estonian
          "fi", // Finnish
          "fr", // French
          "ga", // Irish
          "hr", // Croatian
          "hu", // Hungarian
          "it", // Italian
          "lt", // Lithuanian
          "lv", // Latvian
          "nl", // Dutch
          "no", // Norwegian
          "pl", // Polish
          "pt", // Portuguese
          "ro", // Romanian
          "sk", // Slovak
          "sl", // Slovenian
          "sr", // Serbian
          "sv", // Swedish
          "sw", // Swahili
          "tr"  // Turkish
      };
      for (auto lf : dayFirstLangs)
      {
        if (lang.equalsIgnoreCase(lf))
        {
          return true;
        }
      }
      return false;
    };

    String langForDate = String(language);

    if (langForDate == "ja")
    {
      // Japanese: month number (spaced digits) + day + symbol
      String spacedMonth = "";
      String monthNum = String(timeinfo.tm_mon + 1);
      dateString = monthAbbr + "  " + spacedDay + " ±";
    }
    else
    {
      if (isDayFirst(language))
      {
        dateString = spacedDay + "   " + monthAbbr;
      }
      else
      {
        dateString = monthAbbr + "   " + spacedDay;
      }
    }

    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(0);
    P.print(dateString);

    if (millis() - lastSwitch > weatherDuration)
    {
      advanceDisplayMode();
    }
  }

  yield();
}