#include "globals.h"
#include "src/storage/storage.h"
#include "src/theme/theme.h"
#include "src/wifi/wifi_setup.h"
#include "shared_ui.h"
#include "src/wifi/connection_status.h"
#include "src/idle/idle.h"
#include "src/settings/settings.h"
#include "src/weather/weather.h"
#include "src/time_date/time_date.h"
#include "src/lists/lists.h"
#include "src/lists/lists_screen.h"
#include "src/calendar/calendar.h"
#include "src/calendar/calendar_screen.h"
#include "src/home/home.h"
#include "src/firebase/firebase_sync.h"
#include "src/touch/display_ui.h"

void setup() {
  Serial.begin(115200);
  Serial.println("[Boot] Smart Display starting");

  // load saved settings before the first screen is drawn
  initStorage();
  initTheme();
  initLists();
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
  if (handleLists()) refreshScreen(DATA_LISTS);  // a ticked item was removed
  if (handleWeather()) refreshScreen(DATA_WEATHER);
  if (handleCalendar()) refreshScreen(DATA_CALENDAR);

  VoiceCommand command;
  if (handleFirebase(command)) showVoiceCommand(command);  // opens the list it changed
}
