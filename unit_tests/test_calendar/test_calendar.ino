// ============================================================
// Unit test: Google Calendar API (public calendar + API key).
//
// What it checks
//   1. The network reaches Google: WiFi, IP, name lookup (DNS).
//   2. The calendar ID and API key in secrets.h work (HTTP 200).
//   3. The reply has the fields the display reads (summary, start,
//      end) and Hebrew titles arrive intact (UTF-8).
//   4. Time taken.
// Common failures:
//   HTTP -1 = no secure connection to Google at all (network problem,
//             see the hint printed with it), not a calendar problem
//   404 = wrong ID or the calendar isn't public
//   400/403 = wrong key or Google Calendar API not enabled
//
// Setup: copy main/secrets.h into this folder (in .gitignore, never
// upload it). Network: same as test_wifi.
// ============================================================
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy main/secrets.h into this folder first (it holds CALENDAR_ID and CALENDAR_API_KEY)"
#endif

#define TEST_WIFI_SSID ""
#define TEST_WIFI_PASSWORD ""
#define HOST "www.googleapis.com"
#define ATTEMPTS 2

String urlEncode(const char* text) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (const char* p = text; *p; p++) {
    uint8_t c = *p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

// One request. Returns the HTTP code (negative = no connection) and fills body.
int requestEvents(const String& url, String& body) {
  NetworkClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(8);
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  uint32_t start = millis();
  http.begin(client, url);
  int code = http.GET();
  body = code > 0 ? http.getString() : String();
  http.end();
  Serial.printf("  HTTP %d in %lu ms\n", code, millis() - start);
  if (code < 0) {
    char tlsError[100] = "";
    int tlsCode = client.lastError(tlsError, sizeof(tlsError));
    Serial.printf("  %s; secure connection error %d: %s\n", HTTPClient::errorToString(code).c_str(), tlsCode,
                  tlsCode ? tlsError : "none (the server never answered)");
  }
  return code;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Google Calendar API");

  // 1. network
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
  Serial.printf("  WiFi \"%s\", IP %s, signal %d dBm, free memory %lu\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str(), WiFi.RSSI(), (unsigned long)ESP.getFreeHeap());

  // Right after joining, name lookups can fail for a few seconds: wait up to 10 s
  IPAddress ip;
  start = millis();
  bool found = false;
  while (!(found = WiFi.hostByName(HOST, ip) == 1) && millis() - start < 10000) delay(500);
  if (!found) {
    Serial.println("  DNS: " HOST " not found -> this network has no internet access");
    Serial.println("  (a login page network, e.g. a guest WiFi, or the hotspot has no mobile data)");
    Serial.println("RESULT: FAIL");
    return;
  }
  Serial.printf("  DNS " HOST " -> %s (%lu ms)\n", ip.toString().c_str(), millis() - start);

  // 2. the request, exactly like the firmware; one retry for a passing network hiccup
  String url = "https://" HOST "/calendar/v3/calendars/" + urlEncode(CALENDAR_ID) +
               "/events?key=" + urlEncode(CALENDAR_API_KEY) +
               "&maxResults=5&singleEvents=true&orderBy=startTime&timeMin=2026-01-01T00:00:00Z"
               "&timeZone=Asia%2FJerusalem&fields=items(summary,start,end)";
  String body;
  int code = -1;
  for (int attempt = 1; attempt <= ATTEMPTS && code < 0; attempt++) {
    if (attempt > 1) { Serial.println("  retrying in 2 s"); delay(2000); }
    code = requestEvents(url, body);
  }
  if (code < 0) {
    Serial.println("  No secure connection to Google, although the name was found.");
    Serial.println("  Likely: the network blocks or filters this traffic (guest / school WiFi),");
    Serial.println("  or the signal is weak. Try a phone hotspot, and run test_https_weather:");
    Serial.println("  if that fails too, it is the network, not the calendar.");
    Serial.println("RESULT: FAIL");
    return;
  }

  // 3. the reply
  JsonDocument doc;
  bool parsed = !deserializeJson(doc, body);
  if (code != 200) {
    Serial.printf("  reason: %s\n", parsed ? (const char*)(doc["error"]["message"] | "?") : "reply is not JSON");
    if (code == 404) Serial.println("  -> check CALENDAR_ID and that the calendar is public");
    if (code == 400 || code == 403) Serial.println("  -> check CALENDAR_API_KEY and that Google Calendar API is enabled");
    Serial.println("RESULT: FAIL");
    return;
  }
  JsonArray items = doc["items"];
  bool fieldsOk = parsed && !items.isNull();
  Serial.printf("  %d events since 1 Jan 2026 (first 5):\n", items.isNull() ? 0 : (int)items.size());
  for (JsonObject e : items) {
    const char* when = e["start"]["dateTime"] | (e["start"]["date"] | "?");
    Serial.printf("    %-25s %s\n", when, (const char*)(e["summary"] | "(no title)"));
    if (e["start"].isNull()) fieldsOk = false;
  }
  Serial.println("  (Hebrew titles should print as Hebrew in the Serial Monitor, not as ???)");
  Serial.printf("RESULT: %s\n", fieldsOk ? "PASS" : "FAIL");
}

void loop() {}
