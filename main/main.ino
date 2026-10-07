#include "globals.h"
#include "src/wifi/wifi_setup.h"
#include "shared_ui.h"
#include "src/settings/settings.h"
#include "src/weather/weather.h"
#include "display_ui.h"

void setup() {
  Serial.begin(115200);
  initDisplay();
  connectWiFi();
  fetchWeather();
  drawHomeScreen();
}

void loop() {
  handleTouch();
  checkWeather();
}
