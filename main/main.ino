#include "globals.h"
#include "src/storage/storage.h"
#include "src/wifi/wifi_setup.h"
#include "shared_ui.h"
#include "src/wifi/connection_status.h"
#include "src/idle/idle.h"
#include "src/settings/settings.h"
#include "src/weather/weather.h"
#include "src/time_date/time_date.h"
#include "src/home/home.h"
#include "src/touch/display_ui.h"
#include "src/firebase/firebase_sync.h"

void setup() {
  Serial.begin(115200);
  Serial.println("[Boot] Smart Display starting");

  // load saved settings before the first screen is drawn
  initStorage();
  initWeather();
  initTimeDate();
  initIdle();

  initDisplay();  // loads brightness, shows the home screen
  connectWiFi();
  startFirebase();
}

void loop() {
  handleWiFi();
  handleConnectionStatus();
  handleTimeDate();
  handleTouch();
  handleIdle();
  if (handleWeather()) refreshScreen();
  if (handleFirebase()) refreshScreen();
}
