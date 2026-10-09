#ifndef FIREBASE_SYNC_H
#define FIREBASE_SYNC_H
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../lists/lists.h"

#if __has_include("../../secrets.h")
#include "../../secrets.h"
#else
#error "Missing main/secrets.h - copy main/secrets.example.h to main/secrets.h and fill in the Firebase login (see README)"
#endif

// ============================================================
// Voice commands from the phone app, through Firebase.
//
// The app writes  /command = {type, item, list, runningCount}
//   type: "add" or "remove", list: "shopping" or "todo",
//   runningCount goes up by 1 with every command.
// The app then waits up to 60 s for
//   /status = {runningCount (same number), success, text}
// and shows the user Confirmed / Error / Not confirmed.
//
// This runs in its own FreeRTOS task on core 0, so checking Firebase
// never freezes the touchscreen (loop() runs on core 1).
// It signs in as the display's own Firebase user (Email/Password),
// checks /command every second, applies new commands to the lists
// and writes /status back.
// ============================================================

#define FIREBASE_POLL_MS     1000    // how often /command is checked
#define FIREBASE_RETRY_MS    30000   // wait after a login or database setup error
#define FIREBASE_TASK_STACK  12288   // bytes; raise if the log shows "Stack canary watchpoint triggered (firebase)"

static String fbIdToken;             // proves we're signed in; expires after 1 hour
static String fbRefreshToken;        // used to get a new fbIdToken
static unsigned long fbTokenTime = 0;
static unsigned long fbTokenLifetimeMs = 0;
static long fbLastCount = -1;        // last runningCount handled; -1 = /command not read yet since boot
static String fbPendingStatus;       // /status not delivered yet
static bool fbSetupError = false;    // wrong login or database setup: retry less often
static volatile bool fbListsChanged = false;

// One kept-open (keep-alive) connection to the database, reused every second.
// setInsecure(): the connection is encrypted but the server's certificate
// isn't verified - fine for a course prototype, same as the weather request.
static NetworkClientSecure fbDbClient;
static HTTPClient fbDbHttp;

static String fbDbUrl() {
  String url = FIREBASE_DATABASE_URL;
  if (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
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
  NetworkClientSecure client;
  client.setInsecure();
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
  return deserializeJson(response, payload) == DeserializationError::Ok;
}

static bool fbSignIn() {
  JsonDocument request;
  request["email"] = FIREBASE_USER_EMAIL;
  request["password"] = FIREBASE_USER_PASSWORD;
  request["returnSecureToken"] = true;
  String body;
  serializeJson(request, body);

  JsonDocument response;
  int code = 0;
  bool parsed = fbAuthRequest("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" FIREBASE_API_KEY,
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
  return false;
}

static bool fbRefreshLogin() {
  if (fbRefreshToken.isEmpty()) return false;
  JsonDocument response;
  int code = 0;
  bool parsed = fbAuthRequest("https://securetoken.googleapis.com/v1/token?key=" FIREBASE_API_KEY,
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

// Returns the HTTP code (negative = no connection); the reply body goes into `payload`
static int fbDbRequest(bool isPut, const char* path, const String& body, String& payload) {
  String url = fbDbUrl() + path + ".json?auth=" + fbIdToken;
  if (!fbDbHttp.begin(fbDbClient, url)) return -1;
  fbDbHttp.setTimeout(5000);
  int code;
  if (isPut) {
    fbDbHttp.addHeader("Content-Type", "application/json");
    code = fbDbHttp.PUT(body);
  } else {
    code = fbDbHttp.GET();
  }
  payload = code > 0 ? fbDbHttp.getString() : String();
  fbDbHttp.end();

  if (code == 401 && payload.indexOf("expired") >= 0) {
    fbIdToken = "";  // login expired: sign in again on the next round
  } else if (code == 401 || code == 404) {
    // 401 Permission denied: the database rules don't allow the display's user
    // 404: wrong FIREBASE_DATABASE_URL
    Serial.printf("[Firebase] Database error %d: %s\n", code, payload.c_str());
    fbSetupError = true;
  }
  return code;
}

// Applies one command to the lists and builds the reply for the app
static void fbApplyCommand(const char* type, const char* item, const char* listName,
                           bool& success, String& text) {
  ListId list = listFromName(listName);
  bool isAdd = strcmp(type, "add") == 0;
  bool isRemove = strcmp(type, "remove") == 0;
  ListResult result = LIST_BAD_ITEM;
  if (list != LIST_INVALID && (isAdd || isRemove)) {
    result = isAdd ? addItem(list, item) : removeItem(list, item);
  }

  String label = listLabel(list);
  switch (result) {
    case LIST_ADDED:         success = true;  text = "Added " + String(item) + " to your " + label; break;
    case LIST_ALREADY_THERE: success = true;  text = String(item) + " is already on your " + label; break;
    case LIST_REMOVED:       success = true;  text = "Removed " + String(item) + " from your " + label; break;
    case LIST_NOT_FOUND:     success = false; text = String(item) + " isn't on your " + label; break;
    case LIST_FULL:          success = false; text = "Your " + label + " is full"; break;
    default:                 success = false; text = "The display couldn't understand this command"; break;
  }
  if (result == LIST_ADDED || result == LIST_REMOVED) {
    printList(list);
    fbListsChanged = true;
  }
}

static void fbHandleCommand(const String& payload) {
  static bool listening = false;
  if (!listening) {
    listening = true;
    Serial.println("[Firebase] Listening for commands");
  }

  if (payload == "null") {  // no command yet (or /command was deleted)
    fbLastCount = 0;
    return;
  }
  JsonDocument command;
  if (deserializeJson(command, payload)) {
    Serial.println("[Firebase] Could not read /command");
    return;
  }
  long count = command["runningCount"] | -1L;
  if (count < 0) return;  // not written by the app

  if (fbLastCount < 0) {
    // First read since boot: the command already there was sent while the
    // display was off. The app has already told the user it wasn't confirmed,
    // so applying it now would be confusing.
    fbLastCount = count;
    Serial.printf("[Firebase] Skipped command #%ld from before startup\n", count);
    return;
  }
  // "!=" rather than ">": if someone deletes /command, the app starts again at 1
  if (count == fbLastCount) return;
  fbLastCount = count;

  const char* type = command["type"] | "";
  const char* item = command["item"] | "";
  const char* list = command["list"] | "";
  bool success = false;
  String text;
  fbApplyCommand(type, item, list, success, text);
  Serial.printf("[Firebase] Command #%ld: %s \"%s\" (%s) -> %s\n", count, type, item, list, text.c_str());

  JsonDocument status;
  status["runningCount"] = count;
  status["success"] = success;
  status["text"] = text;
  fbPendingStatus = "";
  serializeJson(status, fbPendingStatus);
}

// Writes the waiting /status. Returns false if it has to be retried.
static bool fbSendPendingStatus() {
  if (fbPendingStatus.isEmpty()) return true;
  String reply;
  int code = fbDbRequest(true, "/status", fbPendingStatus, reply);
  if (code == 200) {
    fbPendingStatus = "";
    return true;
  }
  Serial.printf("[Firebase] Could not write /status (%d), retrying\n", code);
  return false;
}

static void firebaseTask(void*) {
  fbDbClient.setInsecure();
  fbDbHttp.setReuse(true);  // keep the connection open between checks
  int lastErrorCode = 0;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(fbSetupError ? FIREBASE_RETRY_MS : FIREBASE_POLL_MS));
    fbSetupError = false;
    if (WiFi.status() != WL_CONNECTED) continue;

    if (!fbEnsureSignedIn()) {
      fbSetupError = true;  // wrong email/password/API key, or Email/Password sign-in not enabled
      continue;
    }
    if (!fbSendPendingStatus()) continue;

    String payload;
    int code = fbDbRequest(false, "/command", "", payload);
    if (code == 200) {
      if (lastErrorCode != 0) Serial.println("[Firebase] Reconnected");
      lastErrorCode = 0;
      fbHandleCommand(payload);
      fbSendPendingStatus();  // reply right away instead of waiting for the next round
    } else if (code != lastErrorCode) {
      // logged once per new error, not every second (401 and 404 are logged in fbDbRequest)
      if (code < 0) Serial.printf("[Firebase] No connection to the database (%d)\n", code);
      else if (code != 401 && code != 404) Serial.printf("[Firebase] Database error %d\n", code);
      lastErrorCode = code;
    }
  }
}

// ============================================================
// Called from main.ino
// ============================================================

// Once in setup(), after connectWiFi()
void startFirebase() {
  initLists();
  xTaskCreatePinnedToCore(firebaseTask, "firebase", FIREBASE_TASK_STACK, nullptr, 1, nullptr, 0);
  Serial.println("[Firebase] Started");
}

// In loop(). Returns true when a voice command changed a list (the caller redraws the screen).
bool handleFirebase() {
  if (!fbListsChanged) return false;
  fbListsChanged = false;
  return true;
}
#endif
