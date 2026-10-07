#include "globals.h"
#include "src/wifi/wifi_setup.h"
#include "shared_ui.h"
#include "src/wifi/connection_status.h"
#include "src/settings/settings.h"
#include "src/weather/weather.h"
#include "src/home/home.h"
#include "src/touch/display_ui.h"
#include "src/firebase/firebase_sync.h"

void setup() {
  Serial.begin(115200);
  Serial.println("[Boot] Smart Display starting");
  initDisplay();
  drawHomeScreen();
  connectWiFi();
  startFirebase();
}

void loop() {
  handleWiFi();
  handleConnectionStatus();
  handleTouch();
  if (handleWeather()) refreshScreen();
  if (handleFirebase()) refreshScreen();
}
