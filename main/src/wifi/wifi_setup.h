#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <WiFi.h>
#include <FS.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include "../config/config.h"

WiFiManager wifiManager;

// ============================================================
// Setup portal pages (on the phone):
//   WiFi  - choose the network
//   Setup - Firebase login and Google Calendar (config.h); passwords and
//           keys are never shown, leave them empty to keep the saved ones
//   Info  - device details
// The firmware-update page is removed, so nobody can upload firmware from it.
// The setup network has a password (SETUP_AP_PASSWORD).
// The same pages open on the home network from Settings > Device > Setup page.
// ============================================================
#define SETUP_PAGE_MS 600000  // the setup page opened from Settings closes after 10 min

// Password of the SmartDisplay-Setup network (at least 8 characters), so
// people nearby can't join it and change the WiFi or the Firebase login.
// Shown on the display (Settings > Device) while the setup network is open,
// so whoever has the display in hand can always use it.
#define SETUP_AP_NAME     "SmartDisplay-Setup"
#define SETUP_AP_PASSWORD "smartdisplay"

static WiFiManagerParameter* setupParams[6];
static bool configRestartPending = false;
static unsigned long configSavedAt = 0;
static unsigned long setupPageOpenedAt = 0;

static const char* SECRET_FIELD = " type='password' placeholder='saved - leave empty to keep'";
static const char* KEY_FIELD = " placeholder='saved - leave empty to keep'";

static void addSetupParams() {
  static WiFiManagerParameter header("<h3>Voice commands (Firebase)</h3>");
  static WiFiManagerParameter calHeader("<h3>Google Calendar</h3>");
  setupParams[0] = new WiFiManagerParameter("dbUrl", "Database URL", config.dbUrl, sizeof(config.dbUrl) - 1);
  setupParams[1] = new WiFiManagerParameter("apiKey", "Web API key", "", sizeof(config.apiKey) - 1, KEY_FIELD);
  setupParams[2] = new WiFiManagerParameter("email", "Display user email", config.email, sizeof(config.email) - 1);
  setupParams[3] = new WiFiManagerParameter("password", "Display user password", "", sizeof(config.password) - 1, SECRET_FIELD);
  setupParams[4] = new WiFiManagerParameter("calId", "Calendar ID", config.calendarId, sizeof(config.calendarId) - 1);
  setupParams[5] = new WiFiManagerParameter("calKey", "Calendar API key", "", sizeof(config.calendarKey) - 1, KEY_FIELD);
  wifiManager.addParameter(&header);
  for (int i = 0; i < 4; i++) wifiManager.addParameter(setupParams[i]);
  wifiManager.addParameter(&calHeader);
  for (int i = 4; i < 6; i++) wifiManager.addParameter(setupParams[i]);

  wifiManager.setSaveParamsCallback([]() {
    bool changed = false;
    changed |= saveConfigValue("dbUrl", setupParams[0]->getValue(), config.dbUrl, sizeof(config.dbUrl));
    changed |= saveConfigValue("apiKey", setupParams[1]->getValue(), config.apiKey, sizeof(config.apiKey));
    changed |= saveConfigValue("email", setupParams[2]->getValue(), config.email, sizeof(config.email));
    changed |= saveConfigValue("password", setupParams[3]->getValue(), config.password, sizeof(config.password));
    changed |= saveConfigValue("calId", setupParams[4]->getValue(), config.calendarId, sizeof(config.calendarId));
    changed |= saveConfigValue("calKey", setupParams[5]->getValue(), config.calendarKey, sizeof(config.calendarKey));
    Serial.printf("[Config] Setup page saved: %s\n", changed ? "changed, restarting" : "nothing changed");
    if (changed) {
      configRestartPending = true;  // Firebase and the calendar start again with the new values
      configSavedAt = millis();
    }
  });
}

// Opens the setup pages on the home network (Settings > Device). Returns its address.
String openSetupPage() {
  if (WiFi.status() != WL_CONNECTED) return "";
  if (!wifiManager.getWebPortalActive()) {
    wifiManager.startWebPortal();
    Serial.printf("[WiFi] Setup page open at http://%s for 10 min\n", WiFi.localIP().toString().c_str());
  }
  setupPageOpenedAt = millis();
  return "http://" + WiFi.localIP().toString();
}

bool setupPageOpen() {
  return wifiManager.getWebPortalActive();
}

// True once, when new settings were saved (the caller shows a message)
bool takeConfigSavedMessage() {
  static bool shown = false;
  if (!configRestartPending || shown) return false;
  shown = true;
  return true;
}

// Non-blocking: if WiFi isn't available, the setup portal runs in the
// background (see handleWiFi) and the display keeps working offline.
void connectWiFi() {
  addSetupParams();
  const char* menu[] = {"wifi", "param", "info", "exit"};  // no "update" (firmware upload)
  wifiManager.setMenu(menu, 4);
  wifiManager.setTitle("Smart Display");
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(5);  // the touchscreen waits meanwhile; handleWiFi() keeps retrying after
  wifiManager.setAPCallback([](WiFiManager*) {
    Serial.println("[WiFi] Not connected - setup portal \"" SETUP_AP_NAME "\" is open (password: " SETUP_AP_PASSWORD ")");
  });

  if (wifiManager.autoConnect(SETUP_AP_NAME, SETUP_AP_PASSWORD)) {
    Serial.print("[WiFi] Connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Continuing offline");
  }
}

// Called from loop() - keeps the background setup portal running and
// retries the saved network while offline (the portal turns the WiFi
// station off, and the ESP32's own auto-reconnect can give up)
void handleWiFi() {
  wifiManager.process();

  // new Firebase / calendar settings: restart after the phone got the "saved" page
  if (configRestartPending && millis() - configSavedAt > 3000) {
    Serial.println("[Config] Restarting with the new settings");
    delay(100);
    ESP.restart();
  }
  if (wifiManager.getWebPortalActive() && !wifiManager.getConfigPortalActive() &&
      millis() - setupPageOpenedAt > SETUP_PAGE_MS) {
    Serial.println("[WiFi] Setup page closed");
    wifiManager.stopWebPortal();
  }

  static unsigned long offlineSince = 0;
  static unsigned long lastRetry = 0;
  if (WiFi.status() == WL_CONNECTED) {
    offlineSince = 0;
    lastRetry = millis();  // first retry 15 s after a drop, so the ESP32's auto-reconnect goes first
    if (wifiManager.getConfigPortalActive()) {
      Serial.println("[WiFi] Connected - closing setup portal");
      wifiManager.stopConfigPortal();
    }
    return;
  }

  // the setup page opened from Settings only works on the home network
  if (wifiManager.getWebPortalActive() && !wifiManager.getConfigPortalActive()) wifiManager.stopWebPortal();

  // after 1 min offline, open the setup portal so a new network can be chosen
  if (offlineSince == 0) offlineSince = millis();
  if (millis() - offlineSince > 60000 && !wifiManager.getConfigPortalActive()) {
    Serial.println("[WiFi] Offline for 1 min");
    wifiManager.startConfigPortal(SETUP_AP_NAME, SETUP_AP_PASSWORD);
  }

  // retries make the portal unresponsive for a moment, so retry less often while it's open
  unsigned long retryInterval = wifiManager.getConfigPortalActive() ? 60000 : 15000;
  if (millis() - lastRetry < retryInterval) return;
  lastRetry = millis();

  // don't disturb a phone that is using the setup portal
  if (WiFi.softAPgetStationNum() > 0 || !wifiManager.getWiFiIsSaved()) return;
  Serial.println("[WiFi] Retrying saved network");
  WiFi.begin();
}
#endif
