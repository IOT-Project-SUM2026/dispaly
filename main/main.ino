#include "globals.h"
#include "src/health/health.h"
#include "src/config/config.h"
#include "src/net/net.h"
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
  initHealth();   // logs why the board last restarted
  initConfig();   // Firebase login and calendar (setup page, or secrets.h)

  // load saved settings before the first screen is drawn
  initStorage();
  initTheme();
  initHomeLayout();  // which Home widgets are shown, which one is big
  initLists();
  initWeather();
  loadSavedWeather();  // last weather from flash, until the first download
  initCalendar();
  initTimeDate();
  initIdle();
  initSettings();  // which Settings sections are folded

  initDisplay();  // loads brightness, shows the home screen
  connectWiFi();
  startFirebase();
  startWatchdog();  // restarts the board if loop() ever freezes
}

void loop() {
  handleHealth();     // feeds the watchdog, health line in the log every 10 min
  listsCheckTime();   // retries a failed flash write, fixes offline tap times once the clock syncs
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
