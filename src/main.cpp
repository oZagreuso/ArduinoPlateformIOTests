#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>

const char* ssid = "Zagreus";
const char* password = "";

ESP8266WebServer server(80);

const uint8_t LED_PIN = LED_BUILTIN;
bool ledOn = false;

void applyLed() {
  digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
}

String jsonState() {
  String s = "{";
  s += "\"led\":"; s += (ledOn ? "true" : "false");
  s += ",\"ip\":\""; s += WiFi.localIP().toString(); s += "\"";
  s += ",\"rssi\":"; s += WiFi.RSSI();
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

void handleWifiScan() {
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
  server.send(200, "application/json", json);
  WiFi.scanDelete();
}

void initTimeParis() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "fr.pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
}

bool timeSynced() {
  // 1700000000 ~ fin 2023, donc si on est au-dessus, NTP est OK
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
  pinMode(LED_PIN, OUTPUT);
  ledOn = false;
  applyLed();

  Serial.begin(115200);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  initTimeParis();

  // attend un peu la synchro NTP (max ~6s), sans bloquer si ça rate
  for (int i = 0; i < 30 && !timeSynced(); i++) {
    delay(200);
  }
  Serial.print("NTP synced: ");
  Serial.println(timeSynced() ? "YES" : "NO");

  if (!LittleFS.begin()) {
    Serial.println("Erreur LittleFS");
  } else {
    Serial.println("LittleFS OK");
  }

  // Pages (servir explicitement / et /wifi)
  server.on("/", []() { sendFileOr404("/index.html", "text/html; charset=utf-8"); });
  server.on("/wifi", []() { sendFileOr404("/wifi.html", "text/html; charset=utf-8"); });
  server.on("/app.css", []() { sendFileOr404("/app.css", "text/css; charset=utf-8"); });
  server.on("/app.js", []() { sendFileOr404("/app.js", "application/javascript; charset=utf-8"); });

  // API
  server.on("/api/time", []() { server.send(200, "application/json", jsonTime()); });
  server.on("/api/system", []() { server.send(200, "application/json", jsonSystem()); });
  server.on("/api/state", []() { server.send(200, "application/json", jsonState()); });

  server.on("/api/on", []() { ledOn = true; applyLed(); server.send(200, "application/json", jsonState()); });
  server.on("/api/off", []() { ledOn = false; applyLed(); server.send(200, "application/json", jsonState()); });
  server.on("/api/toggle", []() { ledOn = !ledOn; applyLed(); server.send(200, "application/json", jsonState()); });

  server.on("/api/wifi", handleWifiScan);

  server.onNotFound([]() { server.send(404, "text/plain; charset=utf-8", "404"); });

  server.begin();
  Serial.println("Serveur HTTP démarré");
}

void loop() {
  server.handleClient();
}
