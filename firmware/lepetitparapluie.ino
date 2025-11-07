#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>

ESP8266WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
const char* CONFIG_DOMAIN = "lepetitparapluie.config";

const int LED_PIN = LED_BUILTIN;
bool ledOn = false;

bool saveWiFiCredentials(const String& ssid, const String& pass) {
  if (!LittleFS.begin()) return false;
  File f = LittleFS.open("/wifi.txt", "w");
  if (!f) return false;
  f.println(ssid);
  f.println(pass);
  f.close();
  Serial.println("✅ WiFi credentials saved");
  return true;
}

bool loadWiFiCredentials(String& ssid, String& pass) {
  if (!LittleFS.begin()) return false;
  if (!LittleFS.exists("/wifi.txt")) return false;
  File f = LittleFS.open("/wifi.txt", "r");
  if (!f) return false;
  ssid = f.readStringUntil('\n'); ssid.trim();
  pass = f.readStringUntil('\n'); pass.trim();
  f.close();
  return (ssid.length() > 0);
}

void eraseWiFiCredentials() {
  if (!LittleFS.begin()) return;
  if (LittleFS.exists("/wifi.txt")) LittleFS.remove("/wifi.txt");
  Serial.println("🧹 WiFi credentials erased");
}

String ledPage() {
  String stateText = ledOn ? "ON" : "OFF";
  String page = R"rawliteral(
    <html><head><meta charset='UTF-8'><title>lepetitparapluie</title>
    <style>body{font-family:Arial;text-align:center;padding-top:40px;}
    .btn{display:inline-block;padding:10px 20px;margin:10px;font-size:18px;text-decoration:none;border-radius:5px;border:1px solid #444;}
    .on{background:#cfc;}.off{background:#fcc;}
    .small{font-size:14px;margin-top:20px;display:block;}</style></head><body>
  )rawliteral";
  page += "<h1>lepetitparapluie</h1>";
  page += "<p>LED is <b>" + stateText + "</b></p>";
  page += "<p><a class='btn on' href='/on'>ON</a> <a class='btn off' href='/off'>OFF</a></p>";
  page += "<a class='small' href='/reset'>Reset WiFi</a></body></html>";
  return page;
}

void handleRootNormal() { server.send(200, "text/html", ledPage()); }
void handleOn() { ledOn = true; digitalWrite(LED_PIN, LOW); server.send(200, "text/html", ledPage()); }
void handleOff() { ledOn = false; digitalWrite(LED_PIN, HIGH); server.send(200, "text/html", ledPage()); }
void handleReset() { eraseWiFiCredentials(); server.send(200, "text/html", "<html><body><h1>WiFi reset</h1><p>Rebooting...</p></body></html>"); delay(2000); ESP.restart(); }

void startNormalModeServer() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  server.on("/", handleRootNormal);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/reset", handleReset);
  server.begin();
  Serial.println("🌐 lepetitparapluie web server started");
}

String buildNetworkList() {
  int n = WiFi.scanNetworks();
  if (n == 0) return "<option>No networks found</option>";
  String options = "";
  for (int i = 0; i < n; i++) {
    options += "<option value='" + WiFi.SSID(i) + "'>";
    options += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)";
    if (WiFi.encryptionType(i) != ENC_TYPE_NONE) options += " 🔒";
    options += "</option>";
  }
  return options;
}

String configPage(bool showError, const String& errorMsg) {
  String page = R"rawliteral(
    <html><head><meta charset='UTF-8'><title>WiFi Setup</title>
    <style>body{font-family:Arial;text-align:center;padding-top:30px;}
    select,input,button{margin:5px;padding:8px;width:80%;max-width:300px;}
    button{background:#4CAF50;color:white;border:none;border-radius:5px;}
    .error{color:#c00;margin:10px;}</style></head><body><h1>lepetitparapluie WiFi Setup</h1>
  )rawliteral";
  if (showError) page += "<div class='error'>" + errorMsg + "</div>";
  page += "<form action='/save' method='GET'><p><select name='ssid'>";
  page += buildNetworkList();
  page += "</select></p><p><input name='pass' type='password' placeholder='WiFi Password'></p><p><button type='submit'>Connect</button></p></form></body></html>";
  return page;
}

void handleConfigRoot() { server.send(200, "text/html", configPage(false, "")); }

void handleSaveConfig() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() == 0) { server.send(200, "text/html", configPage(true, "SSID required.")); return; }

  Serial.print("🔍 Test connect to "); Serial.println(ssid);
  WiFi.disconnect(); delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { delay(500); Serial.print("."); }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi OK, saving credentials");
    saveWiFiCredentials(ssid, pass);
    server.send(200, "text/html", "<html><body><h1>Connected</h1><p>Rebooting...</p></body></html>");
    delay(2000); ESP.restart();
  } else {
    Serial.println("❌ WiFi failed");
    server.send(200, "text/html", configPage(true, "Connection failed. Try again."));
  }
}

void handleNotFound() { server.sendHeader("Location", String("http://") + CONFIG_DOMAIN, true); server.send(302, "text/plain", ""); }

void startConfigModeAP() {
  WiFi.mode(WIFI_AP_STA);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP("LePetitParapluieSetup");
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", handleConfigRoot);
  server.on("/save", handleSaveConfig);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("📡 Config mode: connect to 'LePetitParapluieSetup' and open http://lepetitparapluie.config");
}

bool connectUsingSavedCredentials() {
  String ssid, pass;
  if (!loadWiFiCredentials(ssid, pass)) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to "); Serial.println(ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Connected");
    Serial.print("📡 IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("❌ Failed to connect");
  return false;
}

void setup() {
  Serial.begin(9600);
  delay(400);
  Serial.println(); Serial.println("🚀 Booting lepetitparapluie...");
  if (connectUsingSavedCredentials()) startNormalModeServer();
  else startConfigModeAP();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
