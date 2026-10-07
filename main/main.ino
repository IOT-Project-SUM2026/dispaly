#include "globals.h"
#include "wifi_setup.h"
#include "shared_ui.h"
#include "settings.h"
#include "weather.h"
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
