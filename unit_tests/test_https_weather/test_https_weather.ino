// ============================================================
// HW unit test: secure internet requests (HTTPS / TLS) using the
// weather download, exactly as the firmware does it.
//
// What it checks
//   1. One HTTPS request for all 5 cities (Open-Meteo): status 200,
//      5 cities in the reply, time taken, memory used by TLS.
//   2. The 8 s handshake limit works: a request to an address that
//      never answers must give up within ~10 s instead of freezing
//      (the default is 120 s).
//
// Network: same as test_wifi (empty = the display's saved network).
// ============================================================
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define TEST_WIFI_SSID ""
#define TEST_WIFI_PASSWORD ""
#define CITY_COUNT 5

const char* URL =
  "https://api.open-meteo.com/v1/forecast?latitude=31.77,32.08,32.79,31.25,29.56"
  "&longitude=35.21,34.78,34.99,34.79,34.95&current_weather=true"
  "&daily=weathercode,temperature_2m_max,temperature_2m_min&forecast_days=4&timezone=Asia%2FJerusalem";
const char* NAMES[CITY_COUNT] = {"Jerusalem", "Tel Aviv", "Haifa", "Beer Sheba", "Eilat"};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] HTTPS: weather for 5 cities in one request + timeout");

  WiFi.mode(WIFI_STA);
  if (strlen(TEST_WIFI_SSID) > 0) WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  else WiFi.begin();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  no WiFi");
    Serial.println("RESULT: FAIL");
    return;
  }

  // 1. the real request
  uint32_t heapBefore = ESP.getFreeHeap();
  NetworkClientSecure client;
  client.setInsecure();           // encrypted, certificate not checked (same as the firmware)
  client.setHandshakeTimeout(8);  // seconds
  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(8000);
  start = millis();
  http.begin(client, URL);
  uint32_t lowestHeap = ESP.getFreeHeap();
  int code = http.GET();
  lowestHeap = min(lowestHeap, (uint32_t)ESP.getFreeHeap());
  String body = code == 200 ? http.getString() : String();
  http.end();
  uint32_t requestMs = millis() - start;
  Serial.printf("  HTTP %d in %lu ms, %u bytes\n", code, requestMs, body.length());
  Serial.printf("  free memory: %lu before, %lu lowest during the request (TLS needs ~40 KB)\n",
                heapBefore, lowestHeap);

  int cities = 0;
  if (code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, body) && doc.is<JsonArray>()) {
      for (JsonObject c : doc.as<JsonArray>()) {
        if (cities < CITY_COUNT) {
          Serial.printf("    %-10s %5.1f C, code %d\n", NAMES[cities], (float)c["current_weather"]["temperature"],
                        (int)c["current_weather"]["weathercode"]);
        }
        cities++;
      }
    }
  }
  bool requestOk = code == 200 && cities == CITY_COUNT;

  // 2. timeout: 10.255.255.1 never answers
  NetworkClientSecure dead;
  dead.setInsecure();
  dead.setHandshakeTimeout(8);
  HTTPClient deadHttp;
  deadHttp.setConnectTimeout(8000);
  deadHttp.setTimeout(8000);
  start = millis();
  deadHttp.begin(dead, "https://10.255.255.1/");
  int deadCode = deadHttp.GET();
  deadHttp.end();
  uint32_t gaveUpMs = millis() - start;
  bool timeoutOk = deadCode < 0 && gaveUpMs < 10000;
  Serial.printf("  unreachable server: gave up after %lu ms with %d (%s) %s\n", gaveUpMs, deadCode,
                HTTPClient::errorToString(deadCode).c_str(), timeoutOk ? "ok" : "TOO SLOW");

  Serial.printf("RESULT: %s\n", requestOk && timeoutOk ? "PASS" : "FAIL");
}

void loop() {}
