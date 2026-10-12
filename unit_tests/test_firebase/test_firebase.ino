// ============================================================
// Unit test: Firebase (login + Realtime Database), the path voice
// commands take to reach the display.
//
// What it checks
//   1. Sign-in with the display's email/password (Firebase Auth).
//   2. Write a test value to /unit_test, read it back, compare.
//   3. Round-trip time of a read (the display checks /command once a
//      second, so a read must take well under 1 s).
//   4. Delete the test value again (the real /command, /status and
//      /lists are not touched).
//
// Setup: copy main/secrets.h into this folder (it is in .gitignore,
// never upload it). Network: same as test_wifi.
// ============================================================
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy main/secrets.h into this folder first (it holds the Firebase login)"
#endif

#define TEST_WIFI_SSID ""
#define TEST_WIFI_PASSWORD ""
#define READS 5

String idToken;

int request(const char* method, const String& url, const String& body, String& reply) {
  NetworkClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(15);
  HTTPClient http;
  http.setReuse(false);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest(method, body);
  reply = code > 0 ? http.getString() : String();
  http.end();
  return code;
}

String dbUrl(const char* path) {
  String url = FIREBASE_DATABASE_URL;
  if (url.endsWith("/")) url.remove(url.length() - 1);
  return url + path + ".json?auth=" + idToken;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Firebase: sign-in + database write / read / delete");

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

  // 1. sign in
  JsonDocument login;
  login["email"] = FIREBASE_USER_EMAIL;
  login["password"] = FIREBASE_USER_PASSWORD;
  login["returnSecureToken"] = true;
  String body, reply;
  serializeJson(login, body);
  start = millis();
  int code = request("POST", "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" FIREBASE_API_KEY,
                     body, reply);
  JsonDocument res;
  deserializeJson(res, reply);
  if (code != 200 || !res["idToken"].is<const char*>()) {
    Serial.printf("  sign-in failed (%d): %s\n", code, (const char*)(res["error"]["message"] | "no response"));
    Serial.println("RESULT: FAIL");
    return;
  }
  idToken = res["idToken"].as<const char*>();
  Serial.printf("  signed in (%lu ms)\n", millis() - start);

  // 2. write + read back
  // random 1..999999: small enough to read back as a normal int
  long value = (long)(esp_random() % 999999) + 1;
  String written = String("{\"value\":") + value + "}";
  code = request("PUT", dbUrl("/unit_test"), written, reply);
  Serial.printf("  write /unit_test: HTTP %d\n", code);
  bool writeOk = code == 200;

  // 3. reads, timed
  uint32_t total = 0, worst = 0;
  bool readOk = writeOk;
  for (int i = 0; i < READS; i++) {
    start = millis();
    code = request("GET", dbUrl("/unit_test"), "", reply);
    uint32_t ms = millis() - start;
    total += ms; worst = max(worst, ms);
    JsonDocument back;
    long got = (code == 200 && !deserializeJson(back, reply)) ? (back["value"] | -1L) : -1L;
    if (got != value) {
      readOk = false;
      Serial.printf("  read %d: HTTP %d, expected %ld, got %ld (reply: %s)\n", i + 1, code, value, got, reply.c_str());
    }
  }
  Serial.printf("  %d reads: value %s, average %lu ms, worst %lu ms\n", READS, readOk ? "matches" : "WRONG",
                total / READS, worst);
  Serial.println("  (each read here opens a new secure connection; the firmware keeps one open, so it is faster)");

  // 4. clean up
  code = request("DELETE", dbUrl("/unit_test"), "", reply);
  Serial.printf("  delete /unit_test: HTTP %d\n", code);

  bool pass = writeOk && readOk && code == 200;
  Serial.printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
}

void loop() {}
