#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Zagreus";
const char* password = "";

ESP8266WebServer server(80);

const uint8_t LED_GREEN = D5;
const uint8_t LED_RED = D6;

bool ledOn = false;

enum SystemMode {
  MODE_BOOTING,
  MODE_WIFI_CONNECTING,
  MODE_WIFI_DISCONNECTED,
  MODE_READY
};

SystemMode systemMode = MODE_BOOTING;

unsigned long lastBlinkMs = 0;
bool blinkState = false;

unsigned long activityUntilMs = 0;

void setLeds(bool green, bool red) {
  digitalWrite(LED_GREEN, green ? HIGH : LOW);
  digitalWrite(LED_RED, red ? HIGH : LOW);
}

void pulseActivity(unsigned long durationMs = 120) {
  activityUntilMs = millis() + durationMs;
}

void updateStatusLeds() {
  unsigned long now = millis();

  if (activityUntilMs > now) {
    setLeds(true, true);
    return;
  }

  switch (systemMode) {
    case MODE_BOOTING:
    case MODE_WIFI_CONNECTING:
      if (now - lastBlinkMs >= 180) {
        lastBlinkMs = now;
        blinkState = !blinkState;
      }
      setLeds(blinkState, !blinkState);
      break;

    case MODE_WIFI_DISCONNECTED:
      if (now - lastBlinkMs >= 400) {
        lastBlinkMs = now;
        blinkState = !blinkState;
      }
      setLeds(false, blinkState);
      break;

    case MODE_READY:
      if (ledOn) {
        setLeds(true, false);
      } else {
        setLeds(false, true);
      }
      break;
  }
}

void applyLed() {
  updateStatusLeds();
}

String jsonState() {
  String s = "{";
  s += "\"led\":"; s += (ledOn ? "true" : "false");
  s += ",\"ip\":\""; s += WiFi.localIP().toString(); s += "\"";
  s += ",\"rssi\":"; s += WiFi.RSSI();
  s += ",\"wifi_connected\":"; s += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  s += "}";
  return s;
}

String jsonSystem() {
  String s = "{";
  s += "\"uptime_s\":"; s += String(millis() / 1000);
  s += ",\"heap\":"; s += String(ESP.getFreeHeap());
  s += ",\"rssi\":"; s += String(WiFi.RSSI());
  s += ",\"ip\":\""; s += WiFi.localIP().toString(); s += "\"";
  s += ",\"chip_id\":"; s += String(ESP.getChipId());
  s += ",\"flash_size\":"; s += String(ESP.getFlashChipSize());
  s += ",\"sdk\":\""; s += ESP.getSdkVersion(); s += "\"";
  s += ",\"reset_reason\":\""; s += ESP.getResetReason(); s += "\"";
  s += "}";
  return s;
}

String getWeather() {
  WiFiClient client;
  HTTPClient http;

  String url =
    "http://api.open-meteo.com/v1/forecast?latitude=48.85&longitude=2.35&current_weather=true";

  if (!http.begin(client, url)) {
    return "{\"error\":\"http.begin failed\"}";
  }

  http.setTimeout(8000);
  http.setUserAgent("ESP8266");

  int httpCode = http.GET();

  if (httpCode <= 0) {
    String err = http.errorToString(httpCode);
    http.end();
    String out = "{\"error\":\"GET failed\",\"code\":";
    out += String(httpCode);
    out += ",\"msg\":\"";
    out += err;
    out += "\"}";
    return out;
  }

  String payload = http.getString();
  http.end();
  return payload;
}

void handleWifiScan() {
  pulseActivity();

  int n = WiFi.scanNetworks();
  String json = "[";

  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{";
    json += "\"ssid\":\""; json += WiFi.SSID(i); json += "\"";
    json += ",\"rssi\":"; json += WiFi.RSSI(i);
    json += ",\"channel\":"; json += WiFi.channel(i);
    json += ",\"secure\":"; json += (WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false");
    json += "}";
  }

  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
  WiFi.scanDelete();
}

void initTimeParis() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "fr.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
}

bool timeSynced() {
  return time(nullptr) > 1700000000;
}

String jsonTime() {
  time_t now = time(nullptr);

  String s = "{";
  s += "\"epoch\":"; s += String((uint32_t)now);
  s += ",\"synced\":"; s += (timeSynced() ? "true" : "false");

  if (timeSynced()) {
    struct tm tmLocal;
    localtime_r(&now, &tmLocal);

    char iso[32];
    strftime(iso, sizeof(iso), "%Y-%m-%d %H:%M:%S", &tmLocal);

    s += ",\"local\":\""; s += iso; s += "\"";
    s += ",\"h\":"; s += String(tmLocal.tm_hour);
    s += ",\"m\":"; s += String(tmLocal.tm_min);
    s += ",\"sec\":"; s += String(tmLocal.tm_sec);
  }

  s += "}";
  return s;
}

void sendFileOr404(const char* path, const char* type) {
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain; charset=utf-8", String("Missing: ") + path);
    return;
  }
  File f = LittleFS.open(path, "r");
  server.streamFile(f, type);
  f.close();
}

void setup() {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  setLeds(false, false);

  Serial.begin(115200);
  Serial.println();

  systemMode = MODE_WIFI_CONNECTING;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    updateStatusLeds();
    delay(120);
    Serial.print(".");
  }

  systemMode = MODE_READY;
  updateStatusLeds();

  Serial.println("\nWiFi connecté");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  initTimeParis();

  for (int i = 0; i < 30 && !timeSynced(); i++) {
    updateStatusLeds();
    delay(200);
  }

  if (!LittleFS.begin()) {
    Serial.println("Erreur LittleFS");
  } else {
    Serial.println("LittleFS OK");
  }

  server.on("/", []() {
    pulseActivity();
    sendFileOr404("/index.html", "text/html; charset=utf-8");
  });

  server.on("/wifi", []() {
    pulseActivity();
    sendFileOr404("/wifi.html", "text/html; charset=utf-8");
  });

  server.on("/console", []() {
    pulseActivity();
    sendFileOr404("/console.html", "text/html; charset=utf-8");
  });

  server.on("/app.css", []() {
    pulseActivity();
    sendFileOr404("/app.css", "text/css; charset=utf-8");
  });

  server.on("/app.js", []() {
    pulseActivity();
    sendFileOr404("/app.js", "application/javascript; charset=utf-8");
  });

  server.on("/api/time", []() {
    pulseActivity();
    server.send(200, "application/json; charset=utf-8", jsonTime());
  });

  server.on("/api/system", []() {
    pulseActivity();
    server.send(200, "application/json; charset=utf-8", jsonSystem());
  });

  server.on("/api/state", []() {
    pulseActivity();
    server.send(200, "application/json; charset=utf-8", jsonState());
  });

  server.on("/api/on", []() {
    pulseActivity();
    ledOn = true;
    applyLed();
    server.send(200, "application/json; charset=utf-8", jsonState());
  });

  server.on("/api/off", []() {
    pulseActivity();
    ledOn = false;
    applyLed();
    server.send(200, "application/json; charset=utf-8", jsonState());
  });

  server.on("/api/toggle", []() {
    pulseActivity();
    ledOn = !ledOn;
    applyLed();
    server.send(200, "application/json; charset=utf-8", jsonState());
  });

  server.on("/api/wifi", handleWifiScan);

  server.on("/api/weather", []() {
    pulseActivity();
    String weather = getWeather();
    server.send(200, "application/json; charset=utf-8", weather);
  });

  server.onNotFound([]() {
    pulseActivity();
    server.send(404, "text/plain; charset=utf-8", "404");
  });

  server.begin();
  Serial.println("Serveur HTTP démarré");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (systemMode != MODE_READY) {
      systemMode = MODE_READY;
    }
  } else {
    if (systemMode != MODE_WIFI_DISCONNECTED) {
      systemMode = MODE_WIFI_DISCONNECTED;
    }
  }

  updateStatusLeds();
  server.handleClient();
}
