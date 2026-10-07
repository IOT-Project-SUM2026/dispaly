#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <WiFi.h>
#include <FS.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

WiFiManager wifiManager;

void connectWiFi() {
  bool connected = wifiManager.autoConnect("SmartDisplay-Setup");

  if (!connected) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
#endif
