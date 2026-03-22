# Smart-ESP-School-Bell-
An ESP8266-based automated school bell controller with a web UI, per-event ring duration, DS3231 RTC backup, and automatic WiFi reconnection. Schedules survive power failures thanks to battery-backed RTC + LittleFS persistent storage.
An ESP8266-based **automated school bell controller** with a web UI, per-event ring duration, DS3231 RTC backup, and automatic WiFi reconnection. Schedules survive power failures thanks to battery-backed RTC + LittleFS persistent storage.

---

## 📸 Features at a Glance

| Feature | Detail |
|---|---|
| ⏰ **Scheduled Bells** | Up to 100 events, per-day, with individual ring durations |
| 🌐 **Web Interface** | Mobile-friendly UI served at `192.168.100.27` |
| 🛰️ **NTP Time Sync** | Auto-syncs from `pool.ntp.org` via WiFi |
| 🔋 **DS3231 RTC Backup** | Keeps time during power cuts & WiFi outages |
| 💾 **Persistent Storage** | Schedule saved to LittleFS (survives reboot) |
| 🔁 **WiFi Auto-Reconnect** | Reconnects every 30s if connection drops |
| 🔔 **Manual Ring** | One-click 5-second manual bell from the UI |
| 📅 **Default Days: Sun–Thu** | Matches Pakistani school week out of the box |

---

## 🧰 Hardware Required

| Component | Purpose |
|---|---|
| **ESP8266** (NodeMCU / Wemos D1 Mini) | Main microcontroller |
| **DS3231 RTC Module** | Battery-backed timekeeping |
| **CR2032 Coin Cell** | Powers DS3231 during outages |
| **5V Relay Module** (Active-Low) | Controls the bell circuit |
| **5V Power Supply** | Powers the ESP8266 |

---

## 🔌 Wiring Diagram

```
ESP8266 Pin    →    Connected To
─────────────────────────────────
D1 (GPIO5)     →    DS3231 SCL
D2 (GPIO4)     →    DS3231 SDA
3.3V           →    DS3231 VCC
GND            →    DS3231 GND

D5 (GPIO14)    →    Relay IN (signal)
5V             →    Relay VCC
GND            →    Relay GND
```

> ⚠️ **Important:** The relay signal pin is `D5`. Do **not** use `D1` for the relay — it is reserved for I2C (SCL) communication with the DS3231 RTC.

---

## 🛠️ Software Setup

### 1. Install Arduino IDE Dependencies

In **Arduino IDE → Library Manager**, install:
- `RTClib` by Adafruit
- `ArduinoJson` by Benoit Blanchon
- `ESP8266 core` (via Board Manager: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`)

LittleFS is included with the ESP8266 core — no separate install needed.

### 2. Configure WiFi & IP

Edit these lines in `smart_bell_rtc.ino`:

```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

IPAddress local_IP(192, 168, 100, 27);   // Change to suit your network
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);
```

### 3. Flash the Firmware

1. Select your board: **Tools → Board → NodeMCU 1.0** (or your variant)
2. Select the correct COM port
3. Click **Upload**

### 4. Access the Web Interface

Open a browser on the same network and navigate to:
```
http://192.168.100.27
```

---

## 📖 How to Use

### Adding a Bell Schedule

1. Open the web interface
2. **Select days** (Mon–Sun checkboxes; Sun–Thu are pre-selected)
3. **Set the time** (hour : minute, AM/PM)
4. **Set duration** in seconds (1–15s)
5. Click **ADD TO SCHEDULE**

> Adding the same day + time again updates the duration instead of creating a duplicate.

### Deleting Events

- Click the **✖** button next to any event to delete it
- Click **DELETE ALL** (with confirmation) to wipe the entire schedule

### Manual Bell

Click the **🔔 Ring (5s)** button in the top-right corner to trigger the bell immediately for 5 seconds.

---

## 🔋 RTC Fallback Behavior

| Scenario | Behavior |
|---|---|
| WiFi connected, NTP available | System time from NTP; RTC synced every hour |
| WiFi drops mid-session | RTC continues keeping time; schedule still fires |
| Power cut, WiFi unavailable on reboot | RTC provides time; schedule fires normally |
| RTC coin cell dead + no WiFi | ⚠️ No time source; bells will not fire until NTP syncs |

The UI footer shows live status (green = NTP, blue = RTC, red = no source).

---

## 📂 Project Structure

```
smart-bell/
├── smart_bell_rtc.ino     # Main Arduino sketch
├── README.md              # This file
├── LICENSE                # MIT License
├── .gitignore             # Arduino / IDE ignores
└── CONTRIBUTING.md        # How to contribute
```

---

## ⚙️ Configuration Reference

| Constant | Default | Description |
|---|---|---|
| `RELAY_PIN` | `D5` | GPIO pin connected to relay module |
| `MY_TZ` | `"PKT-5"` | POSIX timezone string for Pakistan |
| `NTP_SYNC_INTERVAL` | `3600` | Seconds between RTC re-syncs from NTP |
| `WIFI_CHECK_INTERVAL` | `30000` | Milliseconds between WiFi reconnect attempts |
| Max events | `100` | Max scheduled bell events |
| Max ring duration | `15s` | Maximum allowed per-event ring duration |

---

## 🐛 Known Limitations

- The ESP8266 has no true multithreading; heavy WiFi activity may cause very slight (~ms) bell trigger delays.
- If both NTP and RTC are unavailable (dead coin cell + no internet), no bells will fire until one source becomes available.
- LittleFS flash has a finite write endurance (~100,000 cycles). Avoid saving the schedule in a tight loop.

---

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## 📄 License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

---

## 👨‍💻 Author

Developed for **Nasir Higher Secondary School, Rabwah, Pakistan**.  
Feel free to adapt this for any school, masjid, factory, or institution that needs an automated bell or alarm system.
