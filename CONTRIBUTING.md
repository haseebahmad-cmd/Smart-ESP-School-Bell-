# Contributing to Smart Bell System

Thank you for your interest in improving this project! Contributions are welcome — whether it's a bug fix, new feature, or documentation improvement.

---

## 🐛 Reporting Bugs

Before opening an issue, please check:
- The existing [Issues](../../issues) to avoid duplicates
- That you are using the latest version of the code

When reporting a bug, include:
1. **Hardware** — ESP8266 variant (NodeMCU, Wemos D1 Mini, etc.)
2. **Arduino core version** — `Tools → Board → Board Manager`
3. **ArduinoJson / RTClib versions**
4. **What you expected** vs. **what actually happened**
5. **Serial monitor output** if available (set baud to 115200)

---

## 💡 Suggesting Features

Open a GitHub Issue with the label `enhancement` and describe:
- The problem you're trying to solve
- Your proposed solution
- Any hardware changes required

---

## 🔧 Submitting a Pull Request

1. **Fork** the repository
2. **Create a branch** with a descriptive name:
   ```bash
   git checkout -b feature/add-buzzer-support
   git checkout -b fix/midnight-schedule-bug
   ```
3. **Make your changes** — keep commits focused and atomic
4. **Test on real hardware** before submitting
5. **Open a Pull Request** against `main` with:
   - A clear title
   - Description of what changed and why
   - Any wiring changes if hardware is affected

---

## 📐 Code Style Guidelines

- Use `// ---- SECTION ----` comment blocks for major sections (matches existing style)
- Prefix serial debug messages with a tag: `[WiFi]`, `[RTC]`, `[Bell]`, `[FS]`
- Avoid `delay()` in the main loop — use non-blocking `millis()` timers
- All new configuration constants go at the top under `// CONFIGURATION`
- Keep HTML/CSS/JS inside `htmlPage()` using raw string literals `R"====(...)===="`

---

## 🔒 Security Note

**Never commit real WiFi credentials.** Use placeholder values in any PR:
```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

---

## 📄 License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
