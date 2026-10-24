#ifndef FIREBASE_SYNC_H
#define FIREBASE_SYNC_H
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/queue.h>
#include <Preferences.h>
#include "../lists/lists.h"
#include "../config/config.h"  // Firebase login (setup page, or secrets.h)
#include "../net/net.h"

// ============================================================
// Voice commands from the phone app, through Firebase.
//
// The app's server adds every command to a queue:
//   /commands/<id> = {type, item, list, createdAt, answered?}
//   type: "add" or "remove", list: "shopping" or "todo",
//   createdAt: when it was sent (ms, Firebase server time),
//   <id>: Firebase push id, so the ids sort in the order they were sent.
// The display reads the queue every second, applies the commands oldest
// first, and for each one writes the result and removes it from the queue
// in one request:
//   /status/<id> = {success, text, at}   (skipped if "answered" is set)
//   /commands/<id> = null
// Commands sent while the display was off or offline stay in the queue and
// are applied when it is back. Each list item remembers when it last changed,
// so a command older than a tap on the screen is ignored (lists.h).
//
// Every 10 s the display writes /device/lastSeen (server time). When it is
// older than ~25 s the server knows the display is offline and answers the
// app right away from the /lists copy, instead of waiting (it then sets
// "answered" so the display doesn't write a /status nobody reads).
//
// This runs in its own FreeRTOS task on core 0, so checking Firebase
// never freezes the touchscreen (loop() runs on core 1). It signs in as the
// display's own Firebase user (Email/Password). handleFirebase() tells
// loop() which list changed, so the screen can show it.
// It also keeps a copy of both lists in /lists = {shopping: [...], todo: [...]}:
// after every change (voice or touch), and right after WiFi comes back
// if a change was made offline.
// This task never touches the screen: LVGL may only be used from loop().
// ============================================================

#define FIREBASE_POLL_MS       2000    // how often /commands is checked (43,200 reads a day)
#define FIREBASE_HEARTBEAT_MS  10000   // how often /device/lastSeen is written
#define FIREBASE_RETRY_MS      30000   // wait after a login or database setup error
#define FIREBASE_BATCH         10      // commands read per round (the rest next round)
#define FIREBASE_PROTOCOL      "2"     // command format: /commands queue (1 = the old single /command)
#define FIREBASE_TASK_STACK    12288   // bytes; raise if the log shows "Stack canary watchpoint triggered (firebase)"

static String fbIdToken;             // proves we're signed in; expires after 1 hour
static String fbRefreshToken;        // used to get a new fbIdToken
static unsigned long fbTokenTime = 0;
static unsigned long fbTokenLifetimeMs = 0;
static String fbPendingDone;         // result of an applied command not delivered yet (PATCH body)
static bool fbSetupError = false;    // wrong login or database setup: retry less often
static unsigned long fbLastHeartbeat = 0;
static bool fbHeartbeatSent = false;

// A command that was applied, passed from the Firebase task to loop()
struct VoiceCommand {
  ListId list;
  bool onList;                  // true: the item is on the list now (added, or was already there)
  bool success;                 // what the app was told
  char item[LIST_ITEM_LEN];
  char text[80];                // e.g. "Added milk to your shopping list" (shown on the screen)
};
static uint32_t fbUploadedListsVersion = 0;  // lists version last copied to /lists (0 = never)
static QueueHandle_t fbCommandQueue = nullptr;

// One kept-open (keep-alive) connection to the database, reused every second.
// The server's certificate is checked (net.h, TLS_VERIFY_CERTS).
static NetworkClientSecure fbDbClient;
static HTTPClient fbDbHttp;

static String fbDbUrl() {
  return String(config.dbUrl);  // trailing "/" removed by initConfig()
}

static void fbSetTokenLifetime(long expiresInSeconds) {
  fbTokenTime = millis();
  // renew 5 minutes before Firebase says the token expires
  fbTokenLifetimeMs = (unsigned long)max(60L, expiresInSeconds - 300) * 1000UL;
}

// ============================================================
// Login (Firebase Authentication REST API)
// ============================================================

// POSTs to a Google login endpoint. Errors also come back as JSON,
// with the reason in error.message.
static bool fbAuthRequest(const String& url, const String& body, const char* contentType,
                          JsonDocument& response, int& httpCode) {
  fbDbClient.stop();  // only one secure connection at a time (memory)
  NetworkClientSecure client;
  setupSecureClient(client, 15);  // handshake limit 15 s (default 120)
  HTTPClient http;
  http.setReuse(false);
  if (!http.begin(client, url)) {
    httpCode = -1;
    return false;
  }
  http.addHeader("Content-Type", contentType);
  httpCode = http.POST(body);
  String payload = httpCode > 0 ? http.getString() : String();
  http.end();
  netReport(httpCode);
  netLogFailure("Firebase", client, httpCode);
  return deserializeJson(response, payload) == DeserializationError::Ok;
}

static bool fbSignIn() {
  JsonDocument request;
  request["email"] = config.email;
  request["password"] = config.password;
  request["returnSecureToken"] = true;
  String body;
  serializeJson(request, body);

  JsonDocument response;
  int code = 0;
  bool parsed = fbAuthRequest(String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + config.apiKey,
                              body, "application/json", response, code);
  if (code == 200 && parsed && response["idToken"].is<const char*>()) {
    fbIdToken = response["idToken"].as<const char*>();
    fbRefreshToken = response["refreshToken"].as<const char*>();
    fbSetTokenLifetime(String(response["expiresIn"] | "3600").toInt());
    Serial.printf("[Firebase] Signed in (free memory: %u bytes)\n", (unsigned)ESP.getFreeHeap());
    return true;
  }
  const char* reason = parsed ? (response["error"]["message"] | "unknown error") : "no response";
  Serial.printf("[Firebase] Sign-in failed (%d): %s\n", code, reason);
  voiceState = code > 0 ? VOICE_LOGIN_FAILED : VOICE_NO_CONNECTION;
  return false;
}

static bool fbRefreshLogin() {
  if (fbRefreshToken.isEmpty()) return false;
  JsonDocument response;
  int code = 0;
  bool parsed = fbAuthRequest(String("https://securetoken.googleapis.com/v1/token?key=") + config.apiKey,
                              "grant_type=refresh_token&refresh_token=" + fbRefreshToken,
                              "application/x-www-form-urlencoded", response, code);
  if (code == 200 && parsed && response["id_token"].is<const char*>()) {
    fbIdToken = response["id_token"].as<const char*>();
    fbRefreshToken = response["refresh_token"].as<const char*>();
    fbSetTokenLifetime(String(response["expires_in"] | "3600").toInt());
    Serial.println("[Firebase] Login renewed");
    return true;
  }
  Serial.printf("[Firebase] Login renewal failed (%d), signing in again\n", code);
  return false;
}

static bool fbEnsureSignedIn() {
  if (!fbIdToken.isEmpty() && millis() - fbTokenTime < fbTokenLifetimeMs) return true;
  if (!fbIdToken.isEmpty() && fbRefreshLogin()) return true;
  fbIdToken = "";
  return fbSignIn();
}


// ============================================================
// Database (Realtime Database REST API)
// ============================================================

// method: "GET", "PUT" or "PATCH". `query` is added to the URL (e.g. "&limitToFirst=10").
// Returns the HTTP code (negative = no connection); the reply body goes into `payload`.
static int fbDbRequest(const char* method, const char* path, const String& body, String& payload,
                       const char* query = "") {
  String url = fbDbUrl() + path + ".json?auth=" + fbIdToken + query;
  if (!fbDbHttp.begin(fbDbClient, url)) return -1;
  fbDbHttp.setTimeout(5000);
  int code;
  if (strcmp(method, "GET") == 0) {
    code = fbDbHttp.GET();
  } else {
    fbDbHttp.addHeader("Content-Type", "application/json");
    code = fbDbHttp.sendRequest(method, body);
  }
  payload = code > 0 ? fbDbHttp.getString() : String();
  fbDbHttp.end();
  netReport(code);
  if (code < 0) {
    netLogFailure("Firebase", fbDbClient, code);
    setupSecureClient(fbDbClient, 15);  // takes effect if the certificate check was just turned off
  }

  if (code == 401 && payload.indexOf("expired") >= 0) {
    fbIdToken = "";  // login expired: sign in again on the next round
  } else if (code == 401 || code == 404) {
    // 401 Permission denied: the database rules don't allow the display's user
    // 404: wrong FIREBASE_DATABASE_URL
    Serial.printf("[Firebase] Database error %d: %s\n", code, payload.c_str());
    fbSetupError = true;
    voiceState = VOICE_DB_ERROR;
  }
  return code;
}

// Applies one command to the lists and builds the reply for the app
static void fbApplyCommand(const char* type, const char* item, const char* listName, uint32_t at,
                           bool& success, bool& changed, String& text) {
  ListId list = listFromName(listName);
  bool isAdd = strcmp(type, "add") == 0;
  bool isRemove = strcmp(type, "remove") == 0;
  ListResult result = LIST_BAD_ITEM;
  if (list != LIST_INVALID && (isAdd || isRemove) && at > 0) {
    result = applyListCommand(list, item, isAdd, at);
  }

  String label = listLabel(list);
  switch (result) {
    case LIST_ADDED:         success = true;  text = "Added " + String(item) + " to your " + label; break;
    case LIST_ALREADY_THERE: success = true;  text = String(item) + " is already on your " + label; break;
    case LIST_REMOVED:       success = true;  text = "Removed " + String(item) + " from your " + label; break;
    case LIST_NOT_FOUND:     success = false; text = String(item) + " isn't on your " + label; break;
    case LIST_FULL:          success = false; text = "Your " + label + " is full"; break;
    case LIST_OUTDATED:      success = false; text = String(item) + " was changed on the display after this request"; break;
    default:                 success = false; text = "The display couldn't understand this command"; break;
  }
  changed = result == LIST_ADDED || result == LIST_REMOVED;  // the app offers "Undo" only then
  if (changed) printList(list);

  // tell loop() so the screen shows the list (also when nothing changed, e.g. "already there")
  if (result != LIST_BAD_ITEM && result != LIST_OUTDATED) {
    VoiceCommand command;
    command.list = list;
    command.onList = result == LIST_ADDED || result == LIST_ALREADY_THERE;
    command.success = success;
    cleanItemName(command.item, item);
    strncpy(command.text, text.c_str(), sizeof(command.text) - 1);
    command.text[sizeof(command.text) - 1] = '\0';
    xQueueSend(fbCommandQueue, &command, 0);  // if loop() is behind, extra commands are dropped (lists are still updated)
  }
}

// Sends the result of the last applied command: writes /status/<id> and removes
// /commands/<id>, both in one request. Returns false if it has to be retried.
static bool fbSendPendingDone() {
  if (fbPendingDone.isEmpty()) return true;
  String reply;
  int code = fbDbRequest("PATCH", "/", fbPendingDone, reply);  // "/" = the database root
  if (code == 200) {
    fbPendingDone = "";
    return true;
  }
  Serial.printf("[Firebase] Could not confirm a command (%d), retrying\n", code);
  return false;
}

// The last command applied, kept in flash: if the power goes out after a
// command was applied but before its result reached Firebase, it is still in
// the queue after the restart. It is then answered with the saved result
// instead of being applied a second time.
static char fbLastId[48] = "";
static void fbLoadLastCommand() {
  Preferences p;
  if (p.begin("lastcmd", true)) {
    p.getString("id", fbLastId, sizeof(fbLastId));
    p.end();
  }
}
static void fbSaveLastCommand(const char* id, bool success, bool changed, const String& text) {
  Preferences p;
  if (!p.begin("lastcmd", false)) return;
  p.putString("id", id);
  p.putBool("ok", success);
  p.putBool("changed", changed);
  p.putString("text", text);
  p.end();
  strncpy(fbLastId, id, sizeof(fbLastId) - 1);
}
static void fbLoadLastResult(bool& success, bool& changed, String& text) {
  Preferences p;
  if (!p.begin("lastcmd", true)) return;
  success = p.getBool("ok", false);
  changed = p.getBool("changed", false);
  text = p.getString("text", "Done");
  p.end();
}

#define FIREBASE_COMMAND_MAX_AGE_S (7 * 24 * 3600)  // older commands are skipped, not applied

static int fbCompareKeys(const void* a, const void* b) {
  return strcmp(*(const char* const*)a, *(const char* const*)b);
}

// Applies the commands waiting in /commands, oldest first.
// Stops at the first result it can't deliver (retried next round, without applying it again).
static void fbHandleCommands(const String& payload) {
  static bool listening = false;
  if (!listening) {
    listening = true;
    Serial.println("[Firebase] Listening for commands");
  }
  if (payload == "null") return;  // queue empty

  JsonDocument queue;
  if (deserializeJson(queue, payload) || !queue.is<JsonObject>()) {
    Serial.println("[Firebase] Could not read /commands");
    return;
  }
  // push ids sort in the order the commands were sent
  const char* ids[FIREBASE_BATCH];
  int n = 0;
  for (JsonPair pair : queue.as<JsonObject>()) {
    if (n < FIREBASE_BATCH) ids[n++] = pair.key().c_str();
  }
  qsort(ids, n, sizeof(ids[0]), fbCompareKeys);

  for (int i = 0; i < n; i++) {
    JsonObject command = queue[ids[i]];
    const char* type = command["type"] | "";
    const char* item = command["item"] | "";
    const char* list = command["list"] | "";
    uint32_t at = (uint32_t)((command["createdAt"] | 0ULL) / 1000ULL);  // ms -> s
    if (at == 0 && (uint32_t)time(NULL) > LIST_TIME_VALID) at = (uint32_t)time(NULL);  // e.g. added by hand in the console
    bool answered = command["answered"] | false;
    bool success = false, changed = false;
    String text;
    uint32_t now = (uint32_t)time(NULL);
    if (item[0] == '\0' && type[0] == '\0') {
      text = "empty";  // e.g. only "answered" left after the command was removed: just clean up
    } else if (strcmp(ids[i], fbLastId) == 0) {
      fbLoadLastResult(success, changed, text);  // applied before a restart: answer, don't apply again
      Serial.printf("[Firebase] Command %s was applied before the restart: %s\n", ids[i], text.c_str());
    } else if (now > LIST_TIME_VALID && at > 0 && now > at && now - at > FIREBASE_COMMAND_MAX_AGE_S) {
      text = "This request is more than a week old, so it was skipped";  // e.g. the display was away for a long time
      Serial.printf("[Firebase] Command %s skipped: more than a week old\n", ids[i]);
    } else {
      fbApplyCommand(type, item, list, at, success, changed, text);
      Serial.printf("[Firebase] Command %s: %s \"%s\" (%s)%s -> %s\n", ids[i], type, item, list,
                    answered ? " [app already answered]" : "", text.c_str());
      fbSaveLastCommand(ids[i], success, changed, text);
    }

    JsonDocument done;
    if (!answered && item[0] != '\0') {
      JsonObject status = done[String("status/") + ids[i]].to<JsonObject>();
      status["success"] = success;
      status["changed"] = changed;
      status["text"] = text;
      status["at"][".sv"] = "timestamp";  // server time, so the server can clean up old results
    }
    done[String("commands/") + ids[i]] = nullptr;  // remove it from the queue
    fbPendingDone = "";
    serializeJson(done, fbPendingDone);
    if (!fbSendPendingDone()) return;
  }
}

// Writes /device/lastSeen (Firebase server time) every FIREBASE_HEARTBEAT_MS,
// so the app's server knows whether the display is online.
static void fbSendHeartbeat() {
  if (fbHeartbeatSent && millis() - fbLastHeartbeat < FIREBASE_HEARTBEAT_MS) return;
  String reply;
  // protocol: the version of the command format, so the app's server can tell an old firmware
  int code = fbDbRequest("PATCH", "/device", "{\"lastSeen\":{\".sv\":\"timestamp\"},\"protocol\":" FIREBASE_PROTOCOL "}", reply);
  if (code == 200) {
    fbLastHeartbeat = millis();
    fbHeartbeatSent = true;
  }
}

// Copies both lists to /lists when they changed since the last copy.
// Offline changes stay "not copied" and are sent once the connection is back.
static void fbUploadLists() {
  uint32_t version = listsVersion;
  if (version == fbUploadedListsVersion) return;

  JsonDocument doc;
  for (int l = 0; l < LIST_COUNT; l++) {
    JsonArray items = doc[l == LIST_TODO ? "todo" : "shopping"].to<JsonArray>();
    char name[LIST_ITEM_LEN];
    for (int i = 0; listItem((ListId)l, i, name, sizeof(name)); i++) items.add(name);
  }
  String body, reply;
  serializeJson(doc, body);
  int code = fbDbRequest("PUT", "/lists", body, reply);
  if (code == 200) {
    Serial.printf("[Firebase] Lists saved to /lists%s\n", fbUploadedListsVersion == 0 ? " (startup)" : "");
    fbUploadedListsVersion = version;  // changes made meanwhile have a higher version: sent next round
  }
}

// One round: sign in if needed, heartbeat, finished commands, new commands
static void firebaseRound(const String& query, int& lastErrorCode) {
  if (WiFi.status() != WL_CONNECTED || !clockReadyForTls()) {
    fbHeartbeatSent = false;  // say "online" right away when WiFi is back
    return;
  }

  if (!fbEnsureSignedIn()) {
    fbSetupError = true;  // wrong email/password/API key, or Email/Password sign-in not enabled
    return;
  }
  fbSendHeartbeat();
  if (!fbSendPendingDone()) return;
  fbUploadLists();

  String payload;
  int code = fbDbRequest("GET", "/commands", "", payload, query.c_str());
  if (code == 200) {
    if (lastErrorCode != 0) Serial.println("[Firebase] Reconnected");
    lastErrorCode = 0;
    voiceState = VOICE_OK;
    fbHandleCommands(payload);
    fbUploadLists();  // the app's server answers from this copy while the display is offline
  } else if (code != lastErrorCode) {
    // logged once per new error, not every second (401 and 404 are logged in fbDbRequest)
    if (code < 0) {
      Serial.printf("[Firebase] No connection to the database (%d)\n", code);
      voiceState = VOICE_NO_CONNECTION;
    }
    else if (code != 401 && code != 404) Serial.printf("[Firebase] Database error %d\n", code);
    lastErrorCode = code;
  }
}

static void firebaseTask(void*) {
  fbLoadLastCommand();
  setupSecureClient(fbDbClient, 15);  // handshake limit 15 s (default 120): recover sooner if WiFi drops mid-connection
  fbDbHttp.setReuse(true);  // keep the connection open between checks
  int lastErrorCode = 0;
  // oldest commands first ($key = push id); at most FIREBASE_BATCH per round
  String query = String("&orderBy=%22%24key%22&limitToFirst=") + FIREBASE_BATCH;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(fbSetupError ? FIREBASE_RETRY_MS : FIREBASE_POLL_MS));
    fbSetupError = false;
    tlsLock();  // one secure connection at a time (net.h)
    firebaseRound(query, lastErrorCode);
    if (tlsWanted) fbDbClient.stop();  // a weather / calendar download is waiting: free the memory
    tlsUnlock();
  }
}

// ============================================================
// Called from main.ino
// ============================================================

// Once in setup(), after initLists() and connectWiFi()
void startFirebase() {
  fbCommandQueue = xQueueCreate(4, sizeof(VoiceCommand));
  if (!firebaseConfigured()) {
    voiceState = VOICE_NOT_SET_UP;
    Serial.println("[Firebase] Not set up: add the Firebase login on the setup page (or in secrets.h). Voice commands are off");
    return;
  }
  xTaskCreatePinnedToCore(firebaseTask, "firebase", FIREBASE_TASK_STACK, nullptr, 1, &firebaseTaskHandle, 0);
  Serial.println("[Firebase] Started");
}

// In loop(). Returns true when a voice command was applied; `command` says
// which list and item (the caller shows that list).
bool handleFirebase(VoiceCommand& command) {
  return fbCommandQueue && xQueueReceive(fbCommandQueue, &command, 0) == pdTRUE;
}
#endif
