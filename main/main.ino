#include "globals.h"
#include "src/wifi/wifi_setup.h"
#include "shared_ui.h"
#include "src/wifi/connection_status.h"
#include "src/settings/settings.h"
#include "src/weather/weather.h"
#include "src/time_date/time_date.h"
#include "src/home/home.h"
#include "src/touch/display_ui.h"

void setup() {
  Serial.begin(115200);
  Serial.println("[Boot] Smart Display starting");
  initDisplay();  // shows the home screen
  connectWiFi();
}

void loop() {
  handleWiFi();
  handleConnectionStatus();
  handleTimeDate();
  handleTouch();
  if (handleWeather()) refreshScreen();
}
