/****************************************************
   - SMART BELL
   - Static IP: 192.168.100.27
   - Per-Event Duration (Each bell has its own time)
   - DS3231 RTC fallback when WiFi/NTP unavailable
   - Auto WiFi reconnection
   - Fixed relay pin (D5) - D1/D2 freed for I2C (RTC)

   WIRING:
     DS3231 SDA  -> D2 (GPIO4)
     DS3231 SCL  -> D1 (GPIO5)   <-- was relay, now I2C SCL
     Relay       -> D5 (GPIO14)  <-- moved from D1
     DS3231 VCC  -> 3.3V
     DS3231 GND  -> GND
****************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>   // Adafruit RTClib library
#include <time.h>

// ---------------- CONFIGURATION ----------------
const char* ssid     = "your ssid";
const char* password = "password";

// STATIC IP SETTINGS
IPAddress local_IP(192, 168, 100, 27);
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

#define MY_TZ         "PKT-5"
#define RELAY_PIN     D5          // *** MOVED from D1 to free I2C pins ***
#define NTP_SYNC_INTERVAL 3600    // Re-sync RTC from NTP every 1 hour (seconds)

// ---------------- GLOBALS ----------------
ESP8266WebServer server(80);
RTC_DS3231 rtc;
bool rtcAvailable = false;

struct BellEvent {
  int  day;
  int  hour;
  int  minute;
  bool pm;
  bool enabled;
  int  duration;  // seconds
};

BellEvent schedule[100];
int eventCount = 0;

// Bell state
bool          bellActive        = false;
unsigned long bellStartTime     = 0;
int           currentRingDuration = 0;

// Scheduler state
int           lastMinuteChecked = -1;

// NTP->RTC sync tracking
unsigned long lastNtpSyncMillis = 0;
bool          ntpSynced         = false;

// WiFi reconnect
unsigned long lastWifiCheck     = 0;
#define WIFI_CHECK_INTERVAL 30000  // check every 30s

// ---------------------------------------------------------
// TIME HELPERS
// ---------------------------------------------------------

// Returns the best available time_t:
//   1. NTP/system time if synced
//   2. DS3231 RTC if available
//   3. 0 (not available)
time_t getBestTime() {
  time_t now = time(nullptr);
  if (now > 100000UL) {
    return now;  // NTP/system time valid
  }
  if (rtcAvailable) {
    DateTime dt = rtc.now();
    // Convert DateTime to time_t manually
    struct tm t = {};
    t.tm_year = dt.year() - 1900;
    t.tm_mon  = dt.month() - 1;
    t.tm_mday = dt.day();
    t.tm_hour = dt.hour();
    t.tm_min  = dt.minute();
    t.tm_sec  = dt.second();
    t.tm_isdst = 0;
    return mktime(&t);  // Returns UTC+5 since DS3231 is set in PKT
  }
  return 0;
}

// Sync DS3231 from NTP once NTP is available
void syncRtcFromNtp() {
  time_t now = time(nullptr);
  if (now < 100000UL) return;  // NTP not ready yet

  if (!rtcAvailable) return;

  unsigned long ms = millis();
  // Sync on first successful NTP, then every NTP_SYNC_INTERVAL seconds
  if (!ntpSynced || (ms - lastNtpSyncMillis >= (unsigned long)NTP_SYNC_INTERVAL * 1000UL)) {
    struct tm* t = localtime(&now);
    if (!t) return;
    rtc.adjust(DateTime(
      t->tm_year + 1900,
      t->tm_mon  + 1,
      t->tm_mday,
      t->tm_hour,
      t->tm_min,
      t->tm_sec
    ));
    ntpSynced        = true;
    lastNtpSyncMillis = ms;
    Serial.println("[RTC] Synced from NTP");
  }
}

// ---------------------------------------------------------
// SYSTEM FUNCTIONS
// ---------------------------------------------------------
void setupTime() {
  configTime(MY_TZ, "pool.ntp.org", "time.google.com");
}

void loadData() {
  if (!LittleFS.exists("/data.json")) {
    Serial.println("No data.json found, starting with empty schedule.");
    return;
  }

  File f = LittleFS.open("/data.json", "r");
  if (!f) {
    Serial.println("Failed to open data.json");
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    eventCount = 0;
    return;
  }

  JsonArray arr = doc["events"].as<JsonArray>();
  if (arr.isNull()) { eventCount = 0; return; }

  eventCount = 0;
  for (JsonObject o : arr) {
    if (eventCount >= 100) break;
    schedule[eventCount].day      = o["day"]     | 0;
    schedule[eventCount].hour     = o["hour"]    | 8;
    schedule[eventCount].minute   = o["minute"]  | 0;
    schedule[eventCount].pm       = o["pm"]      | false;
    schedule[eventCount].enabled  = o["enabled"] | true;
    schedule[eventCount].duration = o["dur"]     | 3;
    eventCount++;
  }

  Serial.print("Loaded events: ");
  Serial.println(eventCount);
}

void saveData() {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("events");

  for (int i = 0; i < eventCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["day"]     = schedule[i].day;
    o["hour"]    = schedule[i].hour;
    o["minute"]  = schedule[i].minute;
    o["pm"]      = schedule[i].pm;
    o["enabled"] = schedule[i].enabled;
    o["dur"]     = schedule[i].duration;
  }

  File f = LittleFS.open("/data.json", "w");
  if (!f) { Serial.println("Failed to write data.json"); return; }
  serializeJson(doc, f);
  f.close();
}

// ---------------------------------------------------------
// WIFI RECONNECTION
// ---------------------------------------------------------
void maintainWifi() {
  if (millis() - lastWifiCheck < WIFI_CHECK_INTERVAL) return;
  lastWifiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    // Non-blocking: next check in 30s will verify
    setupTime();  // re-init NTP in case it dropped
  }
}

// ---------------------------------------------------------
// BELL LOGIC
// ---------------------------------------------------------
void triggerBell(int durationSecs) {
  if (!bellActive) {
    digitalWrite(RELAY_PIN, LOW);   // Relay ON (active-low)
    bellActive           = true;
    bellStartTime        = millis();
    currentRingDuration  = durationSecs;
    Serial.print("[Bell] Ringing for ");
    Serial.print(durationSecs);
    Serial.println("s");
  }
}

void handleBellState() {
  if (bellActive) {
    if (millis() - bellStartTime >= (unsigned long)currentRingDuration * 1000UL) {
      digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
      bellActive = false;
      Serial.println("[Bell] OFF");
    }
  }
}

// ---------------------------------------------------------
// WEB UI
// ---------------------------------------------------------
String getDayName(int d) {
  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  return (d >= 0 && d <= 6) ? days[d] : "?";
}

// Returns time source label for the UI
String timeSourceLabel() {
  time_t now = time(nullptr);
  if (now > 100000UL) return ntpSynced ? "NTP" : "NTP(sync)";
  if (rtcAvailable)   return "RTC";
  return "No Time";
}

String htmlPage() {
  String p;
  p.reserve(16000);  // BUG FIX: was 6000, too small for many events

  p += R"====(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Bell</title>
<style>
  body { background:#484848; color:white; font-family:'Segoe UI',sans-serif; margin:0; padding:20px; }
  .container { max-width:900px; margin:auto; }
  .header { display:flex; justify-content:space-between; align-items:center; padding-bottom:20px; }
  h1 { margin:0; font-size:22px; font-weight:600; }
  h2 { margin:0; font-size:16px; opacity:.8; }
  .status-panel { text-align:right; }
  .time-display { font-size:18px; font-family:monospace; display:block; margin-bottom:3px; }
  .time-source  { font-size:11px; opacity:.6; display:block; margin-bottom:5px; }
  .btn { border:none; padding:8px 16px; border-radius:20px; color:white; cursor:pointer; font-weight:bold; text-decoration:none; display:inline-block; }
  .btn-purple { background:#673AB7; }
  .btn-purple:hover { background:#7E57C2; }
  .day-selector { display:flex; gap:5px; flex-wrap:wrap; margin-bottom:10px; }
  .day-checkbox { display:none; }
  .day-label { padding:8px 12px; background:#666; border-radius:4px; cursor:pointer; user-select:none; font-size:14px; }
  .day-checkbox:checked + .day-label { background:#673AB7; color:#fff; border:1px solid #9575cd; }
  input, select { background:transparent; border:none; border-bottom:1px solid white; color:white; padding:5px; font-size:16px; text-align:center; }
  select { background:#555; }
  .event-list { background:#333; border-radius:8px; padding:10px; margin-top:20px; }
  .event-item { display:flex; justify-content:space-between; padding:10px; border-bottom:1px solid #555; align-items:center; }
  .time-col { width:120px; font-weight:bold; font-size:1.1em; }
  .dur-col  { width:80px; color:#bbb; }
  .del-btn  { color:#F44336; text-decoration:none; font-weight:bold; margin-left:10px; cursor:pointer; }
  .rtc-badge { background:#1565C0; color:#fff; font-size:10px; padding:2px 6px; border-radius:10px; margin-left:6px; }
  .ntp-badge { background:#2E7D32; color:#fff; font-size:10px; padding:2px 6px; border-radius:10px; margin-left:6px; }
</style>
<script>
  setInterval(function() {
    fetch('/time').then(r=>r.json()).then(d=>{
      document.getElementById('clock').innerText  = d.time;
      document.getElementById('tsrc').innerText   = 'Source: ' + d.source;
      document.getElementById('tsrc').style.color = d.source==='NTP' ? '#66BB6A' : (d.source==='RTC' ? '#42A5F5' : '#EF5350');
    });
  }, 1000);

  function confirmClear() {
    if(confirm("Are you sure you want to DELETE ALL schedules? This cannot be undone.")) {
      location.href="/clear";
    }
  }
</script>
</head><body>
<div class="container">
  <div class="header">
    <div><h1>Nasir Higher Secondary School</h1><h2>Smart Bell System &#8212; Rabwah</h2></div>
    <div class="status-panel">
      <span class="time-display" id="clock">Loading...</span>
      <span class="time-source"  id="tsrc">Source: ...</span>
      <a href="/manual" class="btn btn-purple">&#128276; Ring (5s)</a>
    </div>
  </div>
  <hr style="border:0; border-top:1px solid #777;">

  <div style="background:#3a3a3a; padding:15px; border-radius:8px;">
    <form action="/add" method="GET">
      <div style="margin-bottom:10px; font-weight:bold; color:#ddd;">Select Days:</div>
      <div class="day-selector">
        <input type="checkbox" id="d1" name="d1" class="day-checkbox" checked><label for="d1" class="day-label">Mon</label>
        <input type="checkbox" id="d2" name="d2" class="day-checkbox" checked><label for="d2" class="day-label">Tue</label>
        <input type="checkbox" id="d3" name="d3" class="day-checkbox" checked><label for="d3" class="day-label">Wed</label>
        <input type="checkbox" id="d4" name="d4" class="day-checkbox" checked><label for="d4" class="day-label">Thu</label>
        <input type="checkbox" id="d5" name="d5" class="day-checkbox"><label for="d5" class="day-label">Fri</label>
        <input type="checkbox" id="d6" name="d6" class="day-checkbox"><label for="d6" class="day-label">Sat</label>
        <input type="checkbox" id="d0" name="d0" class="day-checkbox" checked><label for="d0" class="day-label">Sun</label>
      </div>
      <div style="display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-top:15px;">
        <b>Time:</b>
        <input type="number" name="hour"   min="1"  max="12" value="8"  style="width:40px"> :
        <input type="number" name="minute" min="0"  max="59" value="00" style="width:40px">
        <select name="ampm" style="background:#673AB7; border-radius:4px;">
          <option value="0">AM</option><option value="1">PM</option>
        </select>
        <span style="margin-left:10px;"><b>Duration:</b></span>
        <input type="number" name="dur" min="1" max="15" value="3" style="width:40px">s
        <button type="submit" class="btn btn-purple" style="margin-left:auto;">ADD TO SCHEDULE</button>
      </div>
    </form>
  </div>

  <div class="event-list">
    <div style="display:flex; justify-content:space-between; margin-bottom:10px; align-items:center;">
      <b>Scheduled Bells</b>
      <div>
        <span style="margin-right:15px;">Total: )====";

  p += String(eventCount);
  p += R"====(</span>
        <a href="#" onclick="confirmClear()" style="color:#F44336; font-size:12px; border:1px solid #F44336; padding:3px 8px; border-radius:4px; text-decoration:none;">DELETE ALL</a>
      </div>
    </div>
)====";

  for (int i = 0; i < eventCount; i++) {
    p += "<div class='event-item'>";
    p += "<span style='width:50px; color:#bbb;'>" + getDayName(schedule[i].day) + "</span>";
    p += "<span class='time-col'>" + String(schedule[i].hour) + ":";
    if (schedule[i].minute < 10) p += "0";
    p += String(schedule[i].minute) + (schedule[i].pm ? " PM" : " AM") + "</span>";
    p += "<span class='dur-col'>[ " + String(schedule[i].duration) + "s ]</span>";
    p += "<span>";
    if (schedule[i].enabled)
      p += "<span style='color:#4CAF50; font-weight:bold;'>ON</span>";
    else
      p += "<span style='color:#777; font-weight:bold;'>OFF</span>";
    p += "<a href='/del?id=" + String(i) + "' class='del-btn'>&#10006;</a>";
    p += "</span></div>";
  }

  p += "</div>";

  // RTC status footer
  p += "<div style='margin-top:15px; font-size:12px; color:#aaa; background:#2a2a2a; padding:8px 12px; border-radius:6px;'>";
  p += "<b>Time Source:</b> ";
  if (WiFi.status() == WL_CONNECTED) {
    p += "<span style='color:#66BB6A;'>&#9679; WiFi Connected</span>";
  } else {
    p += "<span style='color:#EF5350;'>&#9679; WiFi Offline</span>";
  }
  p += " &nbsp;|&nbsp; <b>RTC:</b> ";
  if (rtcAvailable) {
    p += "<span style='color:#42A5F5;'>&#9679; DS3231 OK</span>";
    if (ntpSynced) p += " <span style='color:#aaa;'>(last synced from NTP)</span>";
    else           p += " <span style='color:#FFA726;'>(NTP not yet synced)</span>";
  } else {
    p += "<span style='color:#EF5350;'>&#9679; Not found</span>";
  }
  p += "</div>";

  p += "</div></body></html>";
  return p;
}

// ---------------------------------------------------------
// ROUTE HANDLERS
// ---------------------------------------------------------
void redirect() {
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

// Returns JSON with time + source so UI can colour-code the clock
void handleTime() {
  time_t now = getBestTime();
  String source = timeSourceLabel();
  String timeStr;

  if (now > 0) {
    struct tm* t = localtime(&now);
    char buf[20];
    strftime(buf, sizeof(buf), "%I:%M:%S %p", t);
    timeStr = String(buf);
  } else {
    timeStr = "--:--:-- --";
  }

  // Return JSON
  String json = "{\"time\":\"" + timeStr + "\",\"source\":\"" + source + "\"}";
  server.send(200, "application/json", json);
}

void handleManual() {
  triggerBell(5);
  redirect();
}

void handleAdd() {
  if (!server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("ampm")) {
    redirect();
    return;
  }

  int dur = 3;
  if (server.hasArg("dur")) {
    dur = server.arg("dur").toInt();
    dur = constrain(dur, 1, 15);
  }

  int  h  = server.arg("hour").toInt();
  int  m  = server.arg("minute").toInt();
  bool pm = (server.arg("ampm").toInt() != 0);

  h = constrain(h, 1, 12);
  m = constrain(m, 0, 59);

  for (int d = 0; d <= 6; d++) {
    String argName = "d" + String(d);
    if (!server.hasArg(argName)) continue;

    // Update if duplicate day+time exists
    bool found = false;
    for (int i = 0; i < eventCount; i++) {
      if (schedule[i].day    == d &&
          schedule[i].hour   == h &&
          schedule[i].minute == m &&
          schedule[i].pm     == pm) {
        schedule[i].duration = dur;
        schedule[i].enabled  = true;
        found = true;
        break;
      }
    }

    if (!found && eventCount < 100) {
      schedule[eventCount] = {d, h, m, pm, true, dur};
      eventCount++;
    }
  }

  saveData();
  redirect();
}

void handleDelete() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if (id >= 0 && id < eventCount) {
      for (int i = id; i < eventCount - 1; i++) schedule[i] = schedule[i + 1];
      eventCount--;
      saveData();
    }
  }
  redirect();
}

void handleClear() {
  eventCount = 0;
  saveData();
  redirect();
}

// ---------------------------------------------------------
// SCHEDULE CHECKER
// ---------------------------------------------------------
void checkSchedule() {
  time_t now = getBestTime();  // Uses RTC if NTP unavailable
  if (now == 0) return;        // No time source at all

  struct tm* t = localtime(&now);
  if (!t) return;

  // Guard: only fire once per minute
  if (t->tm_min == lastMinuteChecked) return;
  lastMinuteChecked = t->tm_min;

  int d = t->tm_wday;
  int h = t->tm_hour;
  int m = t->tm_min;

  for (int i = 0; i < eventCount; i++) {
    if (!schedule[i].enabled || schedule[i].day != d) continue;

    // Convert stored 12hr+pm to 24hr for comparison
    int eh = schedule[i].hour % 12;  // 12->0, others unchanged
    if (schedule[i].pm) eh += 12;

    if (eh == h && schedule[i].minute == m) {
      triggerBell(schedule[i].duration);
      break;  // Only one event per minute
    }
  }
}

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF initially (active-low)

  // Init I2C for DS3231
  Wire.begin();  // SDA=D2, SCL=D1 by default on ESP8266
  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("[RTC] DS3231 found");
    if (rtc.lostPower()) {
      Serial.println("[RTC] WARNING: RTC lost power, time may be wrong until NTP sync!");
      // Time will be corrected on first NTP sync
    } else {
      Serial.println("[RTC] Time retained from battery backup");
      DateTime now = rtc.now();
      Serial.printf("[RTC] Current RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
    }
  } else {
    rtcAvailable = false;
    Serial.println("[RTC] DS3231 NOT found - running on NTP only");
  }

  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed!");
  } else {
    loadData();
  }

  // Configure static IP before WiFi.begin
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("[WiFi] Static IP config failed");
  }

  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());
    setupTime();
  } else {
    Serial.println("\n[WiFi] Failed to connect - running on RTC time only");
  }

  server.on("/",       handleRoot);
  server.on("/time",   handleTime);
  server.on("/manual", handleManual);
  server.on("/add",    handleAdd);
  server.on("/del",    handleDelete);
  server.on("/clear",  handleClear);
  server.begin();
  Serial.println("[HTTP] Server started");
}

// ---------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------
void loop() {
  server.handleClient();
  handleBellState();

  static unsigned long lastScheduleCheck = 0;
  if (millis() - lastScheduleCheck >= 1000UL) {
    lastScheduleCheck = millis();
    checkSchedule();
    syncRtcFromNtp();   // Update RTC if NTP just became available
  }

  maintainWifi();       // Reconnect WiFi if dropped
}
