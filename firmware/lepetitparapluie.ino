#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
const char* CONFIG_DOMAIN = "lepetitparapluie.config";
const int LED_PIN = LED_BUILTIN;

String apiUrl = "https://lepetitparapluie.nicolasvienot.com/api/will-it-rain?location=Brussels";
bool ledOn = false;
unsigned long lastApiCheck = 0;
const unsigned long apiInterval = 5UL * 60UL * 1000UL; // 5 min

#define MAX_LOGS 10
String logs[MAX_LOGS];
int logIndex = 0;

void addLog(const String& entry) {
  logs[logIndex] = entry;
  logIndex = (logIndex + 1) % MAX_LOGS;
  Serial.println(entry);
}

bool saveWiFiCredentials(const String& ssid, const String& pass) {
  if (!LittleFS.begin()) return false;
  File f = LittleFS.open("/wifi.txt", "w");
  if (!f) return false;
  f.println(ssid);
  f.println(pass);
  f.close();
  addLog("✅ WiFi credentials saved");
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
  addLog("🧹 WiFi credentials erased");
}

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

String buildLogsHtml() {
  bool any = false;
  String html = "<ul class='logs'>";
  for (int i = 0; i < MAX_LOGS; i++) {
    int idx = (logIndex + i) % MAX_LOGS;
    if (logs[idx].length() > 0) {
      any = true;
      html += "<li>" + logs[idx] + "</li>";
    }
  }
  html += "</ul>";
  if (!any) return "<p class='empty'>No API calls yet.</p>";
  return html;
}

void handleRootNormal() {
  String page = R"rawliteral(
    <html>
    <head>
      <meta charset='UTF-8'>
      <title>lepetitparapluie</title>
      <style>
        body{font-family:Arial, sans-serif;margin:0;padding:0;background:#f5f5f5;}
        .container{max-width:600px;margin:40px auto;padding:20px;background:white;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.1);text-align:center;}
        h1{margin-top:0;margin-bottom:10px;}
        .subtitle{color:#666;font-size:14px;margin-bottom:20px;}
        .status-badge{display:inline-block;padding:6px 14px;border-radius:999px;font-weight:bold;font-size:14px;margin-top:8px;}
        .on{color:#0a0;}
        .off{color:#c00;}
        .badge-on{background:#e3f8e3;color:#0a0;border:1px solid #b4e3b4;}
        .badge-off{background:#fde3e3;color:#c00;border:1px solid #f2b4b4;}
        hr{border:none;border-top:1px solid #eee;margin:24px 0;}
        .logs-title{margin-bottom:10px;}
        .logs{list-style:none;padding:0;margin:0;text-align:left;font-size:13px;max-height:220px;overflow-y:auto;}
        .logs li{padding:6px 8px;border-bottom:1px solid #eee;}
        .logs li:last-child{border-bottom:none;}
        .empty{color:#888;font-size:13px;}
        .actions{margin-top:20px;display:flex;justify-content:center;gap:10px;flex-wrap:wrap;}
        .btn{display:inline-block;padding:8px 16px;border-radius:4px;text-decoration:none;font-size:14px;border:1px solid transparent;cursor:pointer;}
        .btn-primary{background:#4CAF50;color:white;border-color:#4CAF50;}
        .btn-secondary{background:white;color:#555;border-color:#ccc;}
        .btn:hover{opacity:0.9;}
      </style>
    </head>
    <body>
      <div class="container">
        <h1>lepetitparapluie 🌦️</h1>
        <div class="subtitle">Tiny rain indicator for your day</div>
  )rawliteral";

  page += "<p>Current status:</p>";
  page += "<div class='status-badge " + String(ledOn ? "badge-on" : "badge-off") + "'>";
  page += ledOn ? "RAIN" : "CLEAR";
  page += "</div>";

  page += "<hr>";
  page += "<h3 class='logs-title'>Recent API calls</h3>";
  page += buildLogsHtml();

  page += R"rawliteral(
        <div class="actions">
          <a href="/call" class="btn btn-primary">Refresh now</a>
          <a href="/reset" class="btn btn-secondary">Reset WiFi</a>
        </div>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

void handleReset() {
  eraseWiFiCredentials();
  server.send(200, "text/html", "<html><body><h1>WiFi reset</h1><p>Rebooting...</p></body></html>");
  delay(2000);
  ESP.restart();
}

void callApi();

void handleCall() {
  addLog("🔄 Manual refresh requested");
  callApi();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void startNormalModeServer() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  server.on("/", handleRootNormal);
  server.on("/reset", handleReset);
  server.on("/call", handleCall);
  server.begin();
  addLog("🌐 lepetitparapluie server started");
}

void callApi() {
  if (WiFi.status() != WL_CONNECTED) {
    addLog("⚠️ Not connected to WiFi");
    return;
  }

  HTTPClient http;
  int code = -1;

  if (apiUrl.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);
    http.begin(client, apiUrl);
    code = http.GET();
  } else {
    WiFiClient client;
    http.begin(client, apiUrl);
    code = http.GET();
  }

  if (code > 0) {
    String payload = http.getString();
    addLog("📡 API [" + String(code) + "]: " + payload);

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      bool willRain = doc["will_rain_today"] | false;
      setLed(willRain);
      addLog(String("☁️ will_rain_today: ") + (willRain ? "true" : "false"));
    } else {
      addLog("❌ JSON parse error");
    }
  } else {
    addLog("❌ API call failed: " + String(code));
  }

  http.end();
}

String buildNetworkList() {
  int n = WiFi.scanNetworks();
  if (n == 0) return "<option>No networks found</option>";
  String options = "";
  for (int i = 0; i < n; i++) {
    options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i);
    options += " (" + String(WiFi.RSSI(i)) + " dBm)";
    if (WiFi.encryptionType(i) != ENC_TYPE_NONE) options += " 🔒";
    options += "</option>";
  }
  return options;
}

String configPage(bool showError, const String& msg) {
  String page = R"rawliteral(
    <html><head><meta charset='UTF-8'><title>WiFi Setup</title>
    <style>body{font-family:Arial;text-align:center;padding-top:30px;}
    select,input,button{margin:5px;padding:8px;width:80%;max-width:300px;}
    button{background:#4CAF50;color:white;border:none;border-radius:5px;}
    .error{color:#c00;margin:10px;}</style></head><body>
    <h1>lepetitparapluie WiFi Setup</h1>
  )rawliteral";
  if (showError) page += "<div class='error'>" + msg + "</div>";
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
  WiFi.disconnect(); delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    saveWiFiCredentials(ssid, pass);
    server.send(200, "text/html", "<html><body><h1>Connected</h1><p>Rebooting...</p></body></html>");
    delay(2000); ESP.restart();
  } else {
    server.send(200, "text/html", configPage(true, "Connection failed."));
  }
}

void startConfigModeAP() {
  WiFi.mode(WIFI_AP_STA);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP("LePetitParapluieSetup");
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", handleConfigRoot);
  server.on("/save", handleSaveConfig);
  server.begin();
  addLog("📡 Config mode: connect to 'LePetitParapluieSetup' and open http://lepetitparapluie.config");
}

bool connectUsingSavedCredentials() {
  String ssid, pass;
  if (!loadWiFiCredentials(ssid, pass)) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    addLog("✅ Connected to " + ssid + " | IP: " + WiFi.localIP().toString());
    return true;
  }
  addLog("❌ WiFi connect failed");
  return false;
}

void setup() {
  Serial.begin(9600);
  delay(400);
  Serial.println();
  Serial.println("🚀 Booting lepetitparapluie...");
  if (connectUsingSavedCredentials()) {
    startNormalModeServer();
    callApi();
    lastApiCheck = millis();
  } else {
    startConfigModeAP();
  }
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED && millis() - lastApiCheck > apiInterval) {
    lastApiCheck = millis();
    callApi();
  }
}
