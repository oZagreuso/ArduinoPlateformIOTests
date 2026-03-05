#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

const char* ssid = "Zagreus";
const char* password = "";

ESP8266WebServer server(80);

// LED intégrée (inversée)
const uint8_t LED_PIN = LED_BUILTIN;
bool ledOn = false;

void applyLed() {
  digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
}

String jsonState() {
  String s = "{";
  s += "\"led\":"; 
  s += (ledOn ? "true" : "false");

  s += ",\"ip\":\"";
  s += WiFi.localIP().toString();
  s += "\"";

  s += ",\"rssi\":";
  s += WiFi.RSSI();

  s += "}";
  return s;
}

void handleWifiScan() {

  int n = WiFi.scanNetworks();

  String json = "[";
  
  for (int i = 0; i < n; i++) {

    if (i) json += ",";

    json += "{";

    json += "\"ssid\":\"";
    json += WiFi.SSID(i);
    json += "\"";

    json += ",\"rssi\":";
    json += WiFi.RSSI(i);

    json += ",\"channel\":";
    json += WiFi.channel(i);

    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false");

    json += "}";

  }

  json += "]";

  server.send(200, "application/json", json);

  WiFi.scanDelete();
}

void setup() {

  pinMode(LED_PIN, OUTPUT);
  applyLed();

  Serial.begin(115200);
  Serial.println();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connexion WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connecté");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  // Filesystem
  if (!LittleFS.begin()) {
    Serial.println("Erreur LittleFS");
    return;
  }

  Serial.println("LittleFS OK");

  // fichiers statiques
  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/wifi", LittleFS, "/wifi.html");
  server.serveStatic("/app.css", LittleFS, "/app.css");
  server.serveStatic("/app.js", LittleFS, "/app.js");

  // API
  server.on("/api/state", []() {
    server.send(200, "application/json", jsonState());
  });

  server.on("/api/on", []() {
    ledOn = true;
    applyLed();
    server.send(200, "application/json", jsonState());
  });

  server.on("/api/off", []() {
    ledOn = false;
    applyLed();
    server.send(200, "application/json", jsonState());
  });

  server.on("/api/toggle", []() {
    ledOn = !ledOn;
    applyLed();
    server.send(200, "application/json", jsonState());
  });

  server.on("/api/wifi", handleWifiScan);

  server.onNotFound([]() {
    server.send(404, "text/plain", "404");
  });

  server.begin();

  Serial.println("Serveur HTTP démarré");
}

void loop() {
  server.handleClient();
}