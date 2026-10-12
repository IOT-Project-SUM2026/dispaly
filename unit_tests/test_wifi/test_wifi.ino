// ============================================================
// HW unit test: WiFi radio + connection.
//
// What it checks
//   1. Scan: the radio sees networks (name, signal, channel).
//   2. Connect: time to connect, signal strength (RSSI), IP.
//   3. DNS: the internet names the display needs resolve
//      (Open-Meteo, Firebase, Google Calendar, Technion NTP).
//   4. Reconnect: disconnect on purpose and measure how long the
//      reconnect takes (the display does this after a WiFi drop).
//
// Network: leave TEST_WIFI_SSID empty to use the network the display
// saved through its setup portal (SmartDisplay-Setup), or type one in.
// PASS: connects within 15 s, all names resolve, reconnects within 15 s.
// ============================================================
#include <WiFi.h>

#define TEST_WIFI_SSID ""      // "" = the network saved by the display
#define TEST_WIFI_PASSWORD ""
#define CONNECT_LIMIT_MS 15000

const char* hosts[] = {"api.open-meteo.com", "identitytoolkit.googleapis.com",
                       "www.googleapis.com", "ntp.technion.ac.il"};

// Returns the time it took, or 0 if it didn't connect in time
uint32_t connectWiFi() {
  uint32_t start = millis();
  if (strlen(TEST_WIFI_SSID) > 0) WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  else WiFi.begin();  // saved network
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > CONNECT_LIMIT_MS) return 0;
    delay(100);
  }
  return millis() - start;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] WiFi: scan, connect, DNS, reconnect");
  WiFi.mode(WIFI_STA);

  // 1. scan
  int n = WiFi.scanNetworks();
  Serial.printf("  scan: %d networks\n", n);
  for (int i = 0; i < n && i < 8; i++) {
    Serial.printf("    %-24s %4d dBm  ch %2d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  }
  bool scanOk = n > 0;

  // 2. connect
  uint32_t connectMs = connectWiFi();
  if (!connectMs) {
    Serial.println("  could not connect (no saved network, wrong password, or out of range)");
    Serial.println("RESULT: FAIL");
    return;
  }
  Serial.printf("  connected to \"%s\" in %lu ms, RSSI %d dBm, IP %s\n", WiFi.SSID().c_str(), connectMs,
                WiFi.RSSI(), WiFi.localIP().toString().c_str());

  // 3. DNS
  bool dnsOk = true;
  for (const char* host : hosts) {
    IPAddress ip;
    bool ok = WiFi.hostByName(host, ip) == 1;
    Serial.printf("  DNS %-32s %s\n", host, ok ? ip.toString().c_str() : "FAILED");
    dnsOk = dnsOk && ok;
  }

  // 4. reconnect after a drop
  WiFi.disconnect(false, false);  // drop, keep the saved network
  delay(1000);
  uint32_t reconnectMs = connectWiFi();
  Serial.printf("  reconnect after a drop: %s\n", reconnectMs ? (String(reconnectMs) + " ms").c_str() : "FAILED");

  bool pass = scanOk && dnsOk && reconnectMs;
  Serial.printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
}

void loop() {}
