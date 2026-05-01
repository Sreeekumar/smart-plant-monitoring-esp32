/*
Smart Plant Monitoring System
Features:
- ESP32 based IoT system
- DHT11 (Temp + Humidity)
- Soil Moisture sensing
- IR obstacle detection
- ThingSpeak + Blynk integration
- Web server dashboard
*/

#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL3e8g1k8xP"
#define BLYNK_TEMPLATE_NAME "Smart Plant Monitoring System"
#define BLYNK_AUTH_TOKEN "IFQphSVqAgvItXHBKcBTeiT_sv3t21It"

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>
#include "DHT.h"

/* ---------------- WiFi ---------------- */
const char* ssid     = "Sreee";
const char* password = "123456789";

/* ---------------- ThingSpeak ---------------- */
unsigned long channelID = 3214492;
const char* writeAPIKey = "JLTBK8I31RR3XNS1";
WiFiClient client;

/* ---------------- Pins ---------------- */
#define DHTPIN    4
#define DHTTYPE   DHT11
#define SOIL_PIN  34
#define IR_PIN    27
#define GREEN_LED 19
#define RED_LED   18

/* ---------------- Objects ---------------- */
WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

/* ---------------- Time ---------------- */
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

/* ---------------- Global values ---------------- */
float temperatureC = NAN;
float humidityVal  = NAN;
float soilPct      = 0;
int   irStatus     = 0;

/* ---------------- WEB PAGE (YOUR SAME PAGE) ---------------- */
void handleRoot() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    server.send(200, "text/html", "<h1>Failed to get time</h1>");
    return;
  }

  int hour   = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int second = timeinfo.tm_sec;
  int day    = timeinfo.tm_mday;
  int month  = timeinfo.tm_mon + 1;
  int year   = timeinfo.tm_year + 1900;

  bool isDay = (hour >= 6 && hour < 18);

  String soilStatusText = soilPct > 0 ? "Soil detected" : "Soil not detected";
  String irStatusText   = irStatus ? "Obstacle detected" : "Path clear";

  String page = "";
  page += "<!DOCTYPE html><html><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<meta http-equiv='refresh' content='0.5'>";
  page += "<title>Smart Plant Monitoring System</title>";

  page += "<style>";
  page += "body{margin:0;font-family:Arial,sans-serif;";
  page += isDay ? "background:#e8ffe8;color:#053b1d;"
                : "background:#02140a;color:#dfffe6;";
  page += "display:flex;flex-direction:column;align-items:center;}";
  page += ".container{max-width:420px;width:100%;padding:16px;}";
  page += ".card{background:rgba(255,255,255,0.9);border-radius:16px;padding:14px;margin:10px 0;";
  page += "box-shadow:0 3px 10px rgba(0,0,0,0.15);";
  page += isDay ? "color:#053b1d;" : "color:#02140a;";
  page += "}";
  page += ".header{text-align:center;margin-bottom:12px;}";
  page += ".title{font-size:1.5rem;font-weight:bold;}";
  page += ".subtitle{font-size:0.9rem;opacity:0.8;}";
  page += ".badge{display:inline-block;padding:4px 10px;border-radius:999px;font-size:0.75rem;";
  page += "margin-top:6px;color:white;}";
  page += ".badge-day{background:#34a853;}";
  page += ".badge-night{background:#0b8043;}";
  page += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));grid-gap:10px;}";
  page += ".label{font-size:0.8rem;opacity:0.8;}";
  page += ".value{font-size:1.2rem;font-weight:bold;margin-top:4px;}";
  page += ".status-ok{color:#1b5e20;}";
  page += ".status-warn{color:#b71c1c;}";
  page += "</style></head><body>";

  page += "<div class='container'>";
  page += "<div class='header'>";
  page += "<div class='title'>Smart Plant Monitoring System</div>";
  page += "<div class='subtitle'>IST</div>";
  page += "<div class='subtitle'>";
  page += String(day) + "/" + String(month) + "/" + String(year) + " ";
  page += String(hour) + ":" + String(minute) + ":" + String(second);
  page += "</div>";
  page += "<span class='badge ";
  page += isDay ? "badge-day'>Day mode" : "badge-night'>Night mode";
  page += "</span></div>";

  page += "<div class='card'><h3>Environment</h3><div class='grid'>";
  page += "<div><div class='label'>Temperature</div><div class='value'>" + String(temperatureC,1) + " °C</div></div>";
  page += "<div><div class='label'>Humidity</div><div class='value'>" + String(humidityVal,1) + " %</div></div>";
  page += "</div></div>";

  page += "<div class='card'><h3>Soil</h3><div class='grid'>";
  page += "<div><div class='label'>Status</div><div class='value'>" + soilStatusText + "</div></div>";
  page += "<div><div class='label'>Moisture</div><div class='value'>" + String(soilPct,0) + " %</div></div>";
  page += "</div></div>";

  page += "<div class='card'><h3>Path Status (IR)</h3><div class='grid'>";
  page += "<div><div class='label'>Sensor</div><div class='value'>" + irStatusText + "</div></div>";
  page += "</div></div>";

  page += "</div></body></html>";

  server.send(200, "text/html", page);
}

/* ---------------- READ SENSORS ---------------- */
void readSensors() {
  temperatureC = dht.readTemperature();
  humidityVal  = dht.readHumidity();

  int rawSoil = analogRead(SOIL_PIN);
  rawSoil = constrain(rawSoil, 1000, 3200);
  soilPct = map(rawSoil, 3200, 1000, 0, 100);

  irStatus = (digitalRead(IR_PIN) == LOW);

  digitalWrite(RED_LED, irStatus);
  digitalWrite(GREEN_LED, !irStatus);
}

/* ---------------- SEND TO BLYNK ---------------- */
void sendToBlynk() {
  Blynk.virtualWrite(V0, temperatureC);
  Blynk.virtualWrite(V1, humidityVal);
  Blynk.virtualWrite(V2, soilPct);
  Blynk.virtualWrite(V3, irStatus);
  Blynk.virtualWrite(V4, !irStatus);
}

/* ---------------- SEND TO THINGSPEAK ---------------- */
void sendToThingSpeak() {
  ThingSpeak.setField(1, temperatureC);
  ThingSpeak.setField(2, humidityVal);
  ThingSpeak.setField(3, soilPct);
  ThingSpeak.setField(4, irStatus);
  ThingSpeak.writeFields(channelID, writeAPIKey);
}

/* ---------------- SETUP ---------------- */
void setup() {
  Serial.begin(115200);

  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  retryCount++;

  if (retryCount > 30) {   // ~15 seconds timeout
    Serial.println("\n❌ WiFi connection FAILED");
    Serial.println("Check SSID / Password / Network type");
    return;   // stop setup here
  }
}

  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.begin();

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(3000);

  ThingSpeak.begin(client);

  timer.setInterval(300L, readSensors);
  timer.setInterval(500L, sendToBlynk);
  timer.setInterval(15000L, sendToThingSpeak);
}

/* ---------------- LOOP ---------------- */
void loop() {
  server.handleClient();
  Blynk.run();
  timer.run();
}
