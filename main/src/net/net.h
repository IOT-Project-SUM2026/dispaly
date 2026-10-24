#ifndef NET_H
#define NET_H
#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ============================================================
// Shared by everything that uses the internet (Firebase, weather,
// calendar):
//  - secure connection setup (certificate check)
//  - "is the internet reachable?" (WiFi can be connected while the
//    internet behind it is down, e.g. a hotel login page)
//  - one background task that runs the slow downloads (weather,
//    calendar) on core 0, so the touchscreen never freezes
// ============================================================

// 1: check each server's certificate against the root certificates built
// into the ESP32 core (the same list browsers use). 0: encrypted but not
// checked. If every HTTPS request fails right after an update, try 0.
#define TLS_VERIFY_CERTS 1

// useBuiltinCACertBundle() only exists in newer ESP32 board packages (3.3.x).
// These two overloads pick it when it exists, and otherwise fall back to an
// encrypted connection without the certificate check, so older packages still build.
template <typename C>
auto useRootCertificates(C& client, int) -> decltype(client.useBuiltinCACertBundle(), bool()) {
  client.useBuiltinCACertBundle();
  return true;
}
template <typename C>
bool useRootCertificates(C& client, long) {
  client.setInsecure();
  return false;
}

// ---------- low memory: restart ----------
// A secure connection needs a free block of about 40 KB. If memory breaks
// into small pieces over days of running, every connection fails with
// "SSL - Memory allocation failed" and the display stays offline. A restart
// fixes that in a few seconds, and the lists and settings come back from
// flash, so after TLS_MEMORY_FAILURES_MAX such failures in a row loop()
// restarts the board (health.h).
#define TLS_MEMORY_FAILURES_MAX 5
#define TLS_ERR_MEMORY (-0x7F00)  // MBEDTLS_ERR_SSL_ALLOC_FAILED
static volatile uint8_t tlsMemoryFailures = 0;
volatile bool memoryRestartWanted = false;

// Tasks whose unused stack is printed in the health log (health.h)
TaskHandle_t netTaskHandle = nullptr;
TaskHandle_t firebaseTaskHandle = nullptr;

// Set when the certificate check keeps failing (an old board package, or a
// network that inspects HTTPS): from then on connections stay encrypted but
// the certificate is not checked, so the weather, calendar and voice
// commands keep working. Starts again with the check after a restart.
static volatile bool tlsCheckOff = false;
static volatile uint8_t tlsCertFailures = 0;
#define TLS_CERT_FAILURES_MAX 2

void setupSecureClient(NetworkClientSecure& client, int handshakeTimeoutSec) {
#if TLS_VERIFY_CERTS
  static bool warned = false;
  if (tlsCheckOff) {
    client.setInsecure();
  } else if (!useRootCertificates(client, 0) && !warned) {
    warned = true;
    Serial.println("[Net] This ESP32 board package can't check certificates: update it to 3.3.12 (connections stay encrypted)");
  }
#else
  client.setInsecure();
#endif
  client.setHandshakeTimeout(handshakeTimeoutSec);  // the default (120 s) is far too long
}

// The certificate check needs the right date (a certificate is only valid
// between two dates): before the clock is set from the internet, wait.
bool clockReadyForTls() {
#if TLS_VERIFY_CERTS
  return tlsCheckOff || time(nullptr) > 1700000000;  // after Nov 2023 = set from the internet
#else
  return true;
#endif
}

// After a failed request (httpCode < 0): prints why (the TLS error and the
// free memory), and turns the certificate check off if it is the cause.
void netLogFailure(const char* who, NetworkClientSecure& client, int httpCode) {
  if (httpCode >= 0) return;
  char err[96] = "";
  int tlsCode = client.lastError(err, sizeof(err));
  Serial.printf("[%s] Connection failed: %s (TLS %d: %s), free memory %u, largest block %u\n", who,
                HTTPClient::errorToString(httpCode).c_str(), tlsCode, err[0] ? err : "-",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  if (tlsCode == TLS_ERR_MEMORY && !memoryRestartWanted && ++tlsMemoryFailures >= TLS_MEMORY_FAILURES_MAX) {
    memoryRestartWanted = true;
    Serial.printf("[Net] Out of memory for secure connections %d times in a row: restarting\n", TLS_MEMORY_FAILURES_MAX);
  }
#if TLS_VERIFY_CERTS
  // mbedTLS certificate errors: X509 (-0x2xxx) or "certificate verification failed" (-0x2700)
  bool certProblem = tlsCode <= -0x2000 && tlsCode > -0x3000;
  if (certProblem && !tlsCheckOff && ++tlsCertFailures >= TLS_CERT_FAILURES_MAX) {
    tlsCheckOff = true;
    Serial.println("[Net] Certificate check keeps failing: continuing without it (connections stay encrypted)");
  }
#endif
}

// ---------- internet reachable? ----------
#define NET_DOWN_AFTER_MS 20000  // failing this long (with WiFi) = "No internet"

static volatile unsigned long netFailingSince = 0;  // 0 = last request reached its server
static volatile bool netEverTried = false;

// Every module calls this after a request. httpCode > 0 means a server
// answered (even with an error), so the internet itself works.
void netReport(int httpCode) {
  netEverTried = true;
  if (httpCode > 0) {
    tlsMemoryFailures = 0;
    netFailingSince = 0;
  } else if (netFailingSince == 0) {
    netFailingSince = millis() | 1;  // never 0 while failing
  }
}

// WiFi is connected but no server has answered for NET_DOWN_AFTER_MS
bool internetDown() {
  unsigned long since = netFailingSince;
  return WiFi.status() == WL_CONNECTED && since != 0 && millis() - since > NET_DOWN_AFTER_MS;
}

enum ConnectionState { CONN_OFFLINE, CONN_NO_INTERNET, CONN_OK };

ConnectionState connectionState() {
  if (WiFi.status() != WL_CONNECTED) return CONN_OFFLINE;
  return internetDown() ? CONN_NO_INTERNET : CONN_OK;
}

// ---------- voice commands (written by firebase_sync.h) ----------
enum VoiceState : uint8_t {
  VOICE_STARTING,      // waiting for WiFi / first sign-in
  VOICE_OK,            // listening for commands
  VOICE_NOT_SET_UP,    // no Firebase login saved
  VOICE_LOGIN_FAILED,  // wrong email / password / API key
  VOICE_DB_ERROR,      // database rules or URL wrong
  VOICE_NO_CONNECTION  // can't reach Firebase
};
volatile VoiceState voiceState = VOICE_STARTING;

const char* voiceStateText() {
  switch (voiceState) {
    case VOICE_OK:            return "Working";
    case VOICE_NOT_SET_UP:    return "Not set up";
    case VOICE_LOGIN_FAILED:  return "Sign-in failed";
    case VOICE_DB_ERROR:      return "Database error";
    case VOICE_NO_CONNECTION: return "No connection";
    default:                  return WiFi.status() == WL_CONNECTED ? "Starting..." : "Waiting for WiFi";
  }
}

// ============================================================
// Background downloads
//
// loop() asks for a job with bgRequest() and checks bgTake() on later
// passes. The job function runs in the "net" task and must not touch
// the screen or anything loop() reads: it fills its own buffer, which
// loop() copies after bgTake() says it is done.
// ============================================================
#define NET_TASK_STACK 12288  // bytes; raise if the log shows "Stack canary watchpoint triggered (net)"

enum BgState : uint8_t { BG_IDLE, BG_QUEUED, BG_RUNNING, BG_DONE };

struct BgJob {
  const char* name;
  bool (*run)();             // runs in the net task; returns true on success
  volatile BgState state;
  volatile bool ok;
};

static QueueHandle_t bgQueue = nullptr;

// ---------- one secure connection at a time ----------
// A secure (HTTPS) connection needs about 40 KB of memory while it is open,
// and the board has room for only one next to the screen. Firebase keeps its
// connection open between checks (every second); when a download is waiting
// it closes it (tlsWanted) and gives the turn to the download, then opens it again.
static SemaphoreHandle_t tlsMutex = nullptr;
volatile bool tlsWanted = false;  // a download is waiting for the connection

void tlsInit() {
  if (tlsMutex == nullptr) tlsMutex = xSemaphoreCreateMutex();
}
void tlsLock() {
  tlsInit();
  xSemaphoreTake(tlsMutex, portMAX_DELAY);
}
void tlsUnlock() {
  xSemaphoreGive(tlsMutex);
}

static void netTask(void*) {
  for (;;) {
    BgJob* job;
    if (xQueueReceive(bgQueue, &job, portMAX_DELAY) != pdTRUE) continue;
    job->state = BG_RUNNING;
    tlsWanted = true;  // Firebase closes its connection after its current check
    tlsLock();
    tlsWanted = false;
    bool ok = job->run();
    tlsUnlock();
    job->ok = ok;
    __sync_synchronize();  // results are written before loop() sees "done"
    job->state = BG_DONE;
  }
}

// From loop(). Returns false if the job is already queued or running.
bool bgRequest(BgJob& job) {
  if (job.state != BG_IDLE) return false;
  if (bgQueue == nullptr) {
    bgQueue = xQueueCreate(4, sizeof(BgJob*));
    xTaskCreatePinnedToCore(netTask, "net", NET_TASK_STACK, nullptr, 1, &netTaskHandle, 0);
  }
  job.state = BG_QUEUED;
  BgJob* ptr = &job;
  if (xQueueSend(bgQueue, &ptr, 0) != pdTRUE) {
    job.state = BG_IDLE;
    return false;
  }
  return true;
}

// From loop(). True once when the job has finished; `ok` = its result.
bool bgTake(BgJob& job, bool& ok) {
  if (job.state != BG_DONE) return false;
  __sync_synchronize();
  ok = job.ok;
  job.state = BG_IDLE;
  return true;
}

bool bgBusy(const BgJob& job) {
  return job.state == BG_QUEUED || job.state == BG_RUNNING;
}
#endif
