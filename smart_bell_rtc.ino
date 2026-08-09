/****************************************************
 *  SMART BELL — v2
 *  Nasir Higher Secondary School, Rabwah
 *
 *  ESP8266 + DS3231 RTC automated bell controller.
 *  Static IP, per-event duration, RTC fallback,
 *  auto WiFi reconnect, LittleFS persistence.
 *
 *  ------------------------------------------------
 *  WHAT CHANGED IN v2 (read before flashing)
 *  ------------------------------------------------
 *  1. FIXED — wrong bell times on cold boot w/o WiFi.
 *     v1 only set the TZ (via configTime) *after* a
 *     successful WiFi connect. If the board booted with
 *     no WiFi (power cut + router down), the RTC path's
 *     mktime()/localtime() calls ran with no TZ set and
 *     produced UTC instead of PKT — every bell fired ~5h
 *     off, silently. TZ is now configured unconditionally
 *     at boot, before WiFi is even attempted.
 *
 *  2. FIXED — state-changing routes were plain GET links
 *     (/del, /clear). A link-prefetching browser, a LAN
 *     security scanner, or anyone with the URL could wipe
 *     the schedule with no server-side protection (the
 *     confirm() dialog was client-side only). All mutating
 *     endpoints are now POST-only.
 *
 *  3. FIXED — the "enabled" field existed in the data
 *     model but nothing ever set it false. There was no
 *     way to pause a bell without deleting it. Added a
 *     real enable/disable toggle.
 *
 *  4. ADDED — editing an event in place (previously the
 *     only option was delete + re-add).
 *
 *  5. ADDED — optional short label per event (e.g. "Period
 *     1", "Assembly", "Home Time").
 *
 *  6. REBUILT page delivery for the ESP8266's memory
 *     limits. v1 rebuilt the entire HTML page from ~100+
 *     String concatenations on *every* request, growing
 *     with the event count. Repeated String += on this
 *     chip fragments its small heap and is a common cause
 *     of these boards hanging after days/weeks of uptime.
 *     v2 serves one static HTML/CSS/JS shell straight from
 *     flash (PROGMEM) and exposes a small JSON API; the
 *     browser renders/sorts the event list client-side.
 *
 *  7. ADDED — basic self-healing for a device that runs
 *     unattended for months: a periodic free-heap check
 *     that restarts the board if memory gets critically
 *     low, and a nightly 03:00 restart (skipped if a bell
 *     is mid-ring) to clear any slow memory creep before
 *     it becomes a mid-school-day problem.
 *
 *  8. ADDED — automatic one-time migration of existing
 *     data.json files from v1's 12h+AM/PM format to v2's
 *     internal 24h format, so upgrading does not wipe an
 *     existing schedule.
 *
 *  9. Uses ArduinoJson 7.x (JsonDocument, elastic capacity)
 *     instead of the v6 DynamicJsonDocument API, which is
 *     deprecated as of ArduinoJson 7.
 *
 *  10. ADDED — login for the web panel (username + password,
 *      session cookie, single shared admin account). Plain
 *      HTTP, not HTTPS: this stops casual/opportunistic
 *      access on the LAN, not someone actively sniffing
 *      traffic. Password can be changed from the dashboard
 *      once logged in; "forgot password" is a physical
 *      recovery button (see wiring below) rather than an
 *      email/SMS flow, since this device has no such channel
 *      and adding one (SMTP client, stored mail credentials)
 *      would be a lot of new failure surface for very little
 *      real benefit here.
 *
 *  NOT changed: wiring, relay pin, RTC library, overall
 *  WiFi/NTP/RTC fallback strategy — all of that was sound.
 *
 *  IMPORTANT: this was rewritten and reviewed carefully,
 *  but has not been compiled/flash-tested on real hardware
 *  in this environment. Test on a bench unit before
 *  deploying to the live bell circuit — see README.
 *
 *  WIRING (unchanged from v1):
 *    DS3231 SDA -> D2 (GPIO4)
 *    DS3231 SCL -> D1 (GPIO5)
 *    DS3231 VCC -> 3.3V     DS3231 GND -> GND
 *    Relay IN   -> D5 (GPIO14)   <- do not use D1/D2, reserved for I2C
 *    Relay VCC  -> 5V        Relay GND -> GND
 *
 *  WIRING (new — early-warning LED):
 *    LED anode -> 220-330 ohm resistor -> D7 (GPIO13)
 *    LED cathode -> GND
 *    Behaviour: strobes slowly starting 10s before a scheduled bell,
 *    then strobes faster for the duration of the actual ring.
 *
 *  WIRING (new — password reset button):
 *    Momentary pushbutton -> D6 (GPIO12) and the other leg -> GND
 *    No resistor needed (uses the internal pull-up).
 *    Behaviour: hold for 5s while powering on the board to wipe the
 *    saved password and force it back to ADMIN_PASSWORD_DEFAULT
 *    (defined below). LED flashes 5x fast to confirm it happened.
 *    Optional — the device works fine without this button wired up;
 *    you'd just lose the physical-recovery option if you forget a
 *    changed password.
 ****************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>       // Requires ArduinoJson 7.x
#include <Wire.h>
#include <RTClib.h>            // Adafruit RTClib
#include <time.h>

// ---------------------------------------------------------
// DEBUG SERIAL
// ---------------------------------------------------------
// Set to 1 to re-enable USB serial logging for bench troubleshooting.
// Leave at 0 for the deployed, plug-and-forget unit: this compiles the
// logging out entirely (saves a bit of flash/IRAM, which was already
// close to its ceiling) and leaves the TX/RX pins completely unused.
#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
  #define DBG_BEGIN(baud)   Serial.begin(baud)
  #define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_BEGIN(baud)
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

// ---------------------------------------------------------
// CONFIGURATION — edit these for your deployment
// ---------------------------------------------------------
const char* ssid     = "ssid";
const char* password = "pass";

// Web panel login. Username is fixed; password can be changed later from
// the dashboard (stored in LittleFS) — this is just the factory default,
// and also what the physical recovery button restores it to.
const char* ADMIN_USERNAME = "admin";
const char* ADMIN_PASSWORD_DEFAULT = "admin";

// Static IP settings
IPAddress local_IP(192, 168, 100, 27);
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

#define MY_TZ "PKT-5"                 // POSIX TZ string, no DST in Pakistan
#define RELAY_PIN D5                  // GPIO14 — keep off D1/D2 (I2C)
#define NTP_SYNC_INTERVAL   3600      // seconds between RTC re-syncs from NTP
#define WIFI_CHECK_INTERVAL 30000     // ms between WiFi reconnect attempts
#define MAX_EVENTS 100
#define LABEL_LEN 25                  // 24 visible chars + null terminator
#define MIN_SAFE_HEAP 4000            // bytes; restart if free heap drops below this
#define NIGHTLY_RESTART_HOUR 3        // 03:00 local — outside school hours

#define LED_PIN D7                    // GPIO13 — early-warning strobe LED
#define WARNING_LEAD_SECONDS 10       // start strobing this many seconds before a bell
#define STROBE_INTERVAL_MS     200    // blink speed during the pre-bell warning
#define STROBE_INTERVAL_RING_MS 80    // faster blink speed while the bell is actually ringing

#define RESET_BUTTON_PIN D6           // GPIO12 — hold LOW ~5s at power-up to reset password
#define RESET_HOLD_MS 5000
#define MAX_SESSIONS 3                // concurrent logged-in browsers
#define SESSION_TOKEN_LEN 33          // 32 hex chars + null

// ---------------------------------------------------------
// GLOBALS
// ---------------------------------------------------------
ESP8266WebServer server(80);
RTC_DS3231 rtc;
bool rtcAvailable = false;

struct BellEvent {
  uint8_t day;              // 0=Sun .. 6=Sat
  uint8_t hour;              // 0-23 (24h internal format)
  uint8_t minute;            // 0-59
  uint8_t duration;          // seconds, 1-15
  bool enabled;
  char label[LABEL_LEN];     // optional, may be empty string
};

BellEvent schedule[MAX_EVENTS];
int eventCount = 0;

bool bellActive = false;
unsigned long bellStartTime = 0;
int currentRingDuration = 0;

int lastMinuteChecked = -1;

unsigned long lastNtpSyncMillis = 0;
bool ntpSynced = false;

unsigned long lastWifiCheck = 0;

bool warningActive = false;          // true when a bell is due within WARNING_LEAD_SECONDS
bool ledStrobeState = false;
unsigned long lastStrobeToggle = 0;

char sessionTokens[MAX_SESSIONS][SESSION_TOKEN_LEN];   // zero-initialized (BSS) = all slots empty
int nextSessionSlot = 0;
String currentPassword;   // loaded at boot from LittleFS, falls back to ADMIN_PASSWORD_DEFAULT

// ---------------------------------------------------------
// TIME HELPERS
// ---------------------------------------------------------

// Returns the best available time_t:
//   1. NTP/system time if synced
//   2. DS3231 RTC if available
//   3. 0 if no time source at all
// Both paths are UTC-epoch time_t; the RTC stores local (PKT)
// wall-clock values, and mktime() converts them using the TZ
// configured in setup() — see setupTime().
time_t getBestTime() {
  time_t now = time(nullptr);
  if (now > 100000UL) return now;

  if (rtcAvailable) {
    DateTime dt = rtc.now();
    struct tm t = {};
    t.tm_year = dt.year() - 1900;
    t.tm_mon  = dt.month() - 1;
    t.tm_mday = dt.day();
    t.tm_hour = dt.hour();
    t.tm_min  = dt.minute();
    t.tm_sec  = dt.second();
    t.tm_isdst = 0;
    return mktime(&t);
  }
  return 0;
}

// Sync DS3231 from NTP once NTP is available, then periodically.
void syncRtcFromNtp() {
  time_t now = time(nullptr);
  if (now < 100000UL) return;      // NTP not ready yet
  if (!rtcAvailable) return;

  unsigned long ms = millis();
  if (!ntpSynced || (ms - lastNtpSyncMillis >= (unsigned long)NTP_SYNC_INTERVAL * 1000UL)) {
    struct tm* t = localtime(&now);
    if (!t) return;
    rtc.adjust(DateTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                         t->tm_hour, t->tm_min, t->tm_sec));
    ntpSynced = true;
    lastNtpSyncMillis = ms;
    DBG_PRINTLN("[RTC] Synced from NTP");
  }
}

// Sets the TZ unconditionally (fix #1 above). Safe to call before
// WiFi connects — configTime() sets the TZ synchronously and just
// queues the SNTP request until the network is up.
void setupTime() {
  configTime(MY_TZ, "pool.ntp.org", "time.google.com");
}

// ---------------------------------------------------------
// PERSISTENCE  (LittleFS + ArduinoJson 7)
// ---------------------------------------------------------
void saveData() {
  JsonDocument doc;
  JsonArray arr = doc["events"].to<JsonArray>();
  for (int i = 0; i < eventCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["day"]     = schedule[i].day;
    o["hour"]    = schedule[i].hour;      // 24h format
    o["minute"]  = schedule[i].minute;
    o["enabled"] = schedule[i].enabled;
    o["dur"]     = schedule[i].duration;
    if (schedule[i].label[0] != '\0') o["label"] = schedule[i].label;
  }
  File f = LittleFS.open("/data.json", "w");
  if (!f) { DBG_PRINTLN("[FS] Failed to open data.json for write"); return; }
  serializeJson(doc, f);
  f.close();
}

void loadData() {
  eventCount = 0;
  if (!LittleFS.exists("/data.json")) {
    DBG_PRINTLN("[FS] No data.json found, starting with empty schedule.");
    return;
  }
  File f = LittleFS.open("/data.json", "r");
  if (!f) { DBG_PRINTLN("[FS] Failed to open data.json"); return; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    DBG_PRINT("[FS] JSON parse error: ");
    DBG_PRINTLN(err.c_str());
    return;
  }

  JsonArray arr = doc["events"].as<JsonArray>();
  if (arr.isNull()) return;

  bool legacyFormatFound = false;
  for (JsonObject o : arr) {
    if (eventCount >= MAX_EVENTS) break;
    BellEvent &e = schedule[eventCount];
    e.day = o["day"] | 0;

    if (o["pm"].is<bool>()) {
      // Legacy v1 record: 12h "hour" (1-12) + "pm" bool -> migrate to 24h.
      legacyFormatFound = true;
      int h12 = o["hour"] | 12;
      bool pm = o["pm"] | false;
      e.hour = (h12 % 12) + (pm ? 12 : 0);
    } else {
      e.hour = o["hour"] | 8;             // already 24h format
    }

    e.minute   = o["minute"] | 0;
    e.enabled  = o["enabled"] | true;
    e.duration = o["dur"] | 3;

    const char* lbl = o["label"] | "";
    strncpy(e.label, lbl, LABEL_LEN - 1);
    e.label[LABEL_LEN - 1] = '\0';

    eventCount++;
  }

  DBG_PRINTF("[FS] Loaded %d event(s)%s\n", eventCount,
                legacyFormatFound ? " (migrated from legacy 12h format)" : "");

  if (legacyFormatFound) saveData();      // persist the migration once
}

// Password is stored separately from the schedule so a factory-default
// reset (physical button) can wipe just this file without touching the
// bell schedule. Falls back to the compiled-in default if absent/corrupt.
void loadAuthConfig() {
  currentPassword = ADMIN_PASSWORD_DEFAULT;
  if (!LittleFS.exists("/auth.json")) return;
  File f = LittleFS.open("/auth.json", "r");
  if (!f) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { DBG_PRINTLN("[AUTH] auth.json parse error, using default password"); return; }
  const char* pw = doc["password"] | "";
  if (pw[0] != '\0') currentPassword = String(pw);
}

void saveAuthConfig() {
  JsonDocument doc;
  doc["password"] = currentPassword;
  File f = LittleFS.open("/auth.json", "w");
  if (!f) { DBG_PRINTLN("[AUTH] Failed to open auth.json for write"); return; }
  serializeJson(doc, f);
  f.close();
}


// ---------------------------------------------------------
void maintainWifi() {
  if (millis() - lastWifiCheck < WIFI_CHECK_INTERVAL) return;
  lastWifiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    DBG_PRINTLN("[WiFi] Disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    setupTime();   // harmless if TZ already set; re-queues NTP sync
  }
}

// ---------------------------------------------------------
// BELL
// ---------------------------------------------------------
void triggerBell(int durationSecs) {
  if (!bellActive) {
    digitalWrite(RELAY_PIN, LOW);   // active-low relay: LOW = ON
    bellActive = true;
    bellStartTime = millis();
    currentRingDuration = durationSecs;
    DBG_PRINTF("[Bell] Ringing for %ds\n", durationSecs);
  }
}

void handleBellState() {
  if (bellActive && millis() - bellStartTime >= (unsigned long)currentRingDuration * 1000UL) {
    digitalWrite(RELAY_PIN, HIGH);  // OFF
    bellActive = false;
    DBG_PRINTLN("[Bell] OFF");
  }
}

// ---------------------------------------------------------
// SCHEDULER
// ---------------------------------------------------------
void checkSchedule() {
  time_t now = getBestTime();
  if (now == 0) return;
  struct tm* t = localtime(&now);
  if (!t) return;

  if (t->tm_min == lastMinuteChecked) return;   // only fire once per minute
  lastMinuteChecked = t->tm_min;

  int d = t->tm_wday, h = t->tm_hour, m = t->tm_min;

  for (int i = 0; i < eventCount; i++) {
    if (!schedule[i].enabled || schedule[i].day != d) continue;
    if (schedule[i].hour == h && schedule[i].minute == m) {
      triggerBell(schedule[i].duration);
      break;   // one event per minute
    }
  }
}

// ---------------------------------------------------------
// EARLY-WARNING LED
// ---------------------------------------------------------
// Checked once per second: is any enabled event today due within the
// next WARNING_LEAD_SECONDS? (Doesn't look across midnight — fine for
// school hours, where nothing is scheduled within 10s of 00:00.)
void updateWarningState() {
  time_t now = getBestTime();
  if (now == 0) { warningActive = false; return; }
  struct tm* t = localtime(&now);
  if (!t) { warningActive = false; return; }

  int d = t->tm_wday;
  long nowSec = (long)t->tm_hour * 3600L + (long)t->tm_min * 60L + t->tm_sec;

  bool found = false;
  for (int i = 0; i < eventCount; i++) {
    if (!schedule[i].enabled || schedule[i].day != d) continue;
    long evSec = (long)schedule[i].hour * 3600L + (long)schedule[i].minute * 60L;
    long delta = evSec - nowSec;
    if (delta > 0 && delta <= WARNING_LEAD_SECONDS) { found = true; break; }
  }
  warningActive = found;
}

// Called every loop() iteration — the actual blink timing needs
// sub-second resolution, so this can't live in the 1Hz block.
// Speeds up automatically once bellActive (the ring itself) takes over.
void serviceWarningLed() {
  if (!warningActive && !bellActive) {
    if (ledStrobeState) { ledStrobeState = false; digitalWrite(LED_PIN, LOW); }
    return;
  }
  unsigned long interval = bellActive ? STROBE_INTERVAL_RING_MS : STROBE_INTERVAL_MS;
  unsigned long ms = millis();
  if (ms - lastStrobeToggle >= interval) {
    lastStrobeToggle = ms;
    ledStrobeState = !ledStrobeState;
    digitalWrite(LED_PIN, ledStrobeState ? HIGH : LOW);
  }
}

// ---------------------------------------------------------
// SELF-HEALING
// ---------------------------------------------------------
void checkSystemHealth() {
  static unsigned long lastHeapCheck = 0;
  static int lastRestartYday = -1;

  if (millis() - lastHeapCheck >= 60000UL) {
    lastHeapCheck = millis();
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < MIN_SAFE_HEAP) {
      DBG_PRINTF("[HEALTH] Free heap critically low (%u bytes) - restarting\n", freeHeap);
      delay(200);
      ESP.restart();
    }
  }

  time_t now = getBestTime();
  if (now == 0) return;
  struct tm* t = localtime(&now);
  if (!t) return;

  if (t->tm_hour == NIGHTLY_RESTART_HOUR && t->tm_min == 0 &&
      !bellActive && lastRestartYday != t->tm_yday) {
    lastRestartYday = t->tm_yday;
    DBG_PRINTLN("[HEALTH] Scheduled nightly restart");
    delay(200);
    ESP.restart();
  }
}

// ---------------------------------------------------------
// AUTH  (session-cookie login, single shared admin account)
// ---------------------------------------------------------
// Not encrypted transport (plain HTTP) — this stops casual/opportunistic
// access on the LAN, not a determined attacker sniffing traffic. See the
// wiring note at the top of this file for the physical recovery button.

String extractSessionToken() {
  if (!server.hasHeader("Cookie")) return "";
  String cookie = server.header("Cookie");
  int idx = cookie.indexOf("session=");
  if (idx == -1) return "";
  idx += 8;
  int end = cookie.indexOf(';', idx);
  String token = (end == -1) ? cookie.substring(idx) : cookie.substring(idx, end);
  token.trim();
  return token;
}

bool isAuthenticated() {
  String token = extractSessionToken();
  if (token.length() == 0) return false;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (sessionTokens[i][0] != '\0' && token.equals(sessionTokens[i])) return true;
  }
  return false;
}

String generateSessionToken() {
  const char* hexChars = "0123456789abcdef";
  char buf[SESSION_TOKEN_LEN];
  for (int i = 0; i < SESSION_TOKEN_LEN - 1; i++) buf[i] = hexChars[random(16)];
  buf[SESSION_TOKEN_LEN - 1] = '\0';
  return String(buf);
}

void clearAllSessions() {
  for (int i = 0; i < MAX_SESSIONS; i++) sessionTokens[i][0] = '\0';
}

void sendUnauthorized() {
  JsonDocument doc;
  doc["ok"] = false;
  doc["message"] = "Not authenticated";
  String out;
  serializeJson(doc, out);
  server.send(401, "application/json", out);
}

void redirectToLogin() {
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "");
}

#include "login_page.h"

void handleLoginPage() {
  server.send_P(200, "text/html", LOGIN_HTML);
}

void handleLoginPost() {
  String u = server.hasArg("username") ? server.arg("username") : "";
  String p = server.hasArg("password") ? server.arg("password") : "";

  if (u == ADMIN_USERNAME && p == currentPassword) {
    String token = generateSessionToken();
    strncpy(sessionTokens[nextSessionSlot], token.c_str(), SESSION_TOKEN_LEN - 1);
    sessionTokens[nextSessionSlot][SESSION_TOKEN_LEN - 1] = '\0';
    nextSessionSlot = (nextSessionSlot + 1) % MAX_SESSIONS;

    server.sendHeader("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  } else {
    server.sendHeader("Location", "/login?error=1");
    server.send(302, "text/plain", "");
  }
}

void handleLogout() {
  String token = extractSessionToken();
  if (token.length() > 0) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
      if (sessionTokens[i][0] != '\0' && token.equals(sessionTokens[i])) { sessionTokens[i][0] = '\0'; break; }
    }
  }
  server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
  redirectToLogin();
}

void handleChangePassword() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (!server.hasArg("current") || !server.hasArg("newpass")) { sendResult(false, "Missing fields"); return; }
  if (server.arg("current") != currentPassword) { sendResult(false, "Current password is incorrect"); return; }
  String np = server.arg("newpass");
  if (np.length() < 4) { sendResult(false, "New password must be at least 4 characters"); return; }
  currentPassword = np;
  saveAuthConfig();
  sendResult(true);
}

// Checked once at boot, before WiFi. Hold the button LOW (D6 to GND) for
// RESET_HOLD_MS to wipe the stored password and any active sessions,
// restoring ADMIN_PASSWORD_DEFAULT — this is the "forgot password" path.
// Requires physical access to the device, same as WiFi/relay wiring does.
void checkPasswordResetButton() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(RESET_BUTTON_PIN) != LOW) return;

  DBG_PRINTLN("[AUTH] Reset button held at boot - keep holding to confirm...");
  unsigned long start = millis();
  while (millis() - start < RESET_HOLD_MS) {
    if (digitalRead(RESET_BUTTON_PIN) != LOW) {
      DBG_PRINTLN("[AUTH] Button released early - reset cancelled");
      return;
    }
    delay(50);
  }

  DBG_PRINTLN("[AUTH] Resetting password to factory default");
  if (LittleFS.exists("/auth.json")) LittleFS.remove("/auth.json");
  currentPassword = ADMIN_PASSWORD_DEFAULT;
  clearAllSessions();

  // Confirm visually even with no serial connected.
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH); delay(120);
    digitalWrite(LED_PIN, LOW);  delay(120);
  }
}

// ---------------------------------------------------------
// WEB UI  (static PROGMEM shell, no per-request rebuilding)
// ---------------------------------------------------------
#include "page.h"


// ---------------------------------------------------------
// ROUTE HANDLERS
// ---------------------------------------------------------
void handleRoot() {
  if (!isAuthenticated()) { redirectToLogin(); return; }
  server.send_P(200, "text/html", PAGE_HTML);
}

String timeSourceLabel() {
  time_t now = time(nullptr);
  if (now > 100000UL) return ntpSynced ? "NTP" : "NTP(sync)";
  if (rtcAvailable) return "RTC";
  return "No Time";
}

void handleApiStatus() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  time_t now = getBestTime();
  struct tm* t = (now > 0) ? localtime(&now) : nullptr;

  JsonDocument doc;
  if (t) {
    doc["valid"]   = true;
    doc["hour"]    = t->tm_hour;
    doc["minute"]  = t->tm_min;
    doc["second"]  = t->tm_sec;
    doc["weekday"] = t->tm_wday;
  } else {
    doc["valid"] = false;
  }
  doc["source"]     = timeSourceLabel();
  doc["wifi"]       = (WiFi.status() == WL_CONNECTED);
  doc["rtc"]        = rtcAvailable;
  doc["ntpSynced"]  = ntpSynced;
  doc["eventCount"] = eventCount;
  doc["bellActive"] = bellActive;
  doc["freeHeap"]   = ESP.getFreeHeap();
  doc["uptimeSec"]  = millis() / 1000;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleApiEvents() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < eventCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"]      = i;
    o["day"]     = schedule[i].day;
    o["hour"]    = schedule[i].hour;
    o["minute"]  = schedule[i].minute;
    o["enabled"] = schedule[i].enabled;
    o["duration"]= schedule[i].duration;
    o["label"]   = schedule[i].label;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void sendResult(bool ok, const char* msg) {
  JsonDocument doc;
  doc["ok"] = ok;
  if (msg[0] != '\0') doc["message"] = msg;
  String out;
  serializeJson(doc, out);
  server.send(ok ? 200 : 400, "application/json", out);
}
void sendResult(bool ok) { sendResult(ok, ""); }

static void setLabel(char* dest, const String& src) {
  String s = src;
  if (s.length() > LABEL_LEN - 1) s = s.substring(0, LABEL_LEN - 1);
  strncpy(dest, s.c_str(), LABEL_LEN - 1);
  dest[LABEL_LEN - 1] = '\0';
}

void handleApiAdd() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (!server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("ampm")) {
    sendResult(false, "Missing fields");
    return;
  }
  int dur = server.hasArg("dur") ? constrain(server.arg("dur").toInt(), 1, 15) : 3;
  int h12 = constrain(server.arg("hour").toInt(), 1, 12);
  int m   = constrain(server.arg("minute").toInt(), 0, 59);
  bool pm = server.arg("ampm").toInt() != 0;
  int h24 = (h12 % 12) + (pm ? 12 : 0);
  String label = server.hasArg("label") ? server.arg("label") : "";

  int added = 0, updated = 0;
  bool full = false;

  for (int d = 0; d <= 6; d++) {
    if (!server.hasArg("d" + String(d))) continue;

    bool found = false;
    for (int i = 0; i < eventCount; i++) {
      if (schedule[i].day == d && schedule[i].hour == h24 && schedule[i].minute == m) {
        schedule[i].duration = dur;
        schedule[i].enabled = true;
        setLabel(schedule[i].label, label);
        found = true;
        updated++;
        break;
      }
    }
    if (!found) {
      if (eventCount >= MAX_EVENTS) { full = true; continue; }
      BellEvent &e = schedule[eventCount];
      e.day = d; e.hour = h24; e.minute = m; e.duration = dur; e.enabled = true;
      setLabel(e.label, label);
      eventCount++;
      added++;
    }
  }

  if (added == 0 && updated == 0) {
    sendResult(false, full ? "Schedule full (100 events max)" : "Select at least one day");
    return;
  }
  saveData();
  sendResult(true);
}

void handleApiUpdate() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (!server.hasArg("id")) { sendResult(false, "Missing id"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= eventCount) { sendResult(false, "Invalid id"); return; }

  if (server.hasArg("day")) schedule[id].day = constrain(server.arg("day").toInt(), 0, 6);
  if (server.hasArg("hour") && server.hasArg("ampm")) {
    int h12 = constrain(server.arg("hour").toInt(), 1, 12);
    bool pm = server.arg("ampm").toInt() != 0;
    schedule[id].hour = (h12 % 12) + (pm ? 12 : 0);
  }
  if (server.hasArg("minute")) schedule[id].minute = constrain(server.arg("minute").toInt(), 0, 59);
  if (server.hasArg("dur"))    schedule[id].duration = constrain(server.arg("dur").toInt(), 1, 15);
  if (server.hasArg("label"))  setLabel(schedule[id].label, server.arg("label"));

  saveData();
  sendResult(true);
}

void handleApiToggle() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (!server.hasArg("id")) { sendResult(false, "Missing id"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= eventCount) { sendResult(false, "Invalid id"); return; }
  schedule[id].enabled = !schedule[id].enabled;
  saveData();
  sendResult(true);
}

void handleApiDelete() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (!server.hasArg("id")) { sendResult(false, "Missing id"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= eventCount) { sendResult(false, "Invalid id"); return; }
  for (int i = id; i < eventCount - 1; i++) schedule[i] = schedule[i + 1];
  eventCount--;
  saveData();
  sendResult(true);
}

void handleApiClear() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  eventCount = 0;
  saveData();
  sendResult(true);
}

void handleApiRing() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  triggerBell(5);
  sendResult(true);
}

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void setup() {
  DBG_BEGIN(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // relay OFF initially (active-low)

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);      // LED off initially (active-high)

  // Note: no randomSeed() call here on purpose. On the ESP8266 core,
  // random() draws from the hardware RNG by default; calling randomSeed()
  // switches it to a deterministic software sequence instead, which would
  // make session tokens *more* predictable, not less.

  // Mount LittleFS first — both the schedule and the auth config live here,
  // and the reset button needs it before WiFi/RTC are touched.
  bool fsOk = LittleFS.begin();
  if (!fsOk) DBG_PRINTLN("[FS] LittleFS mount failed!");

  // Hold D6 to GND for 5s at power-up to restore the default password.
  // Blocking here is fine — this only runs if someone is deliberately
  // holding a button during boot.
  if (fsOk) checkPasswordResetButton();

  loadAuthConfig();
  if (fsOk) loadData();

  // Set TZ unconditionally, before WiFi — see "WHAT CHANGED IN v2" #1.
  setupTime();

  Wire.begin();   // SDA=D2, SCL=D1
  if (rtc.begin()) {
    rtcAvailable = true;
    DBG_PRINTLN("[RTC] DS3231 found");
    if (rtc.lostPower()) {
      DBG_PRINTLN("[RTC] WARNING: lost power, time may be wrong until NTP sync");
    } else {
      DateTime now = rtc.now();
      DBG_PRINTF("[RTC] Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    }
  } else {
    rtcAvailable = false;
    DBG_PRINTLN("[RTC] DS3231 NOT found - running on NTP only");
  }

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    DBG_PRINTLN("[WiFi] Static IP config failed");
  }
  WiFi.begin(ssid, password);
  DBG_PRINT("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    DBG_PRINT(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    DBG_PRINTLN("\n[WiFi] Connected: " + WiFi.localIP().toString());
  } else {
    DBG_PRINTLN("\n[WiFi] Failed to connect - running on RTC time only");
  }

  server.collectHeaders("Cookie");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/api/account/password", HTTP_POST, handleChangePassword);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/events", HTTP_GET, handleApiEvents);
  server.on("/api/events/add", HTTP_POST, handleApiAdd);
  server.on("/api/events/update", HTTP_POST, handleApiUpdate);
  server.on("/api/events/toggle", HTTP_POST, handleApiToggle);
  server.on("/api/events/delete", HTTP_POST, handleApiDelete);
  server.on("/api/events/clear", HTTP_POST, handleApiClear);
  server.on("/api/ring", HTTP_POST, handleApiRing);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

  server.begin();
  DBG_PRINTLN("[HTTP] Server started");
}

// ---------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------
void loop() {
  server.handleClient();
  handleBellState();
  serviceWarningLed();   // needs sub-second resolution, runs every iteration

  static unsigned long lastTick = 0;
  if (millis() - lastTick >= 1000UL) {
    lastTick = millis();
    checkSchedule();
    syncRtcFromNtp();
    checkSystemHealth();
    updateWarningState();
  }

  maintainWifi();
}
