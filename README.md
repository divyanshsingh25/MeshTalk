# 🔒 MeshTalk — Offline Encrypted Chat over ESP8266

> A complete offline chat system that lets multiple devices communicate without internet using a WiFi network created by the ESP8266. All messages are **encrypted** with a shared passphrase — the ESP8266 only relays ciphertext and never sees your plaintext.

---

## 📁 Project Structure

```
meshtalk/
├── meshtalk.ino          ← Arduino sketch (everything embedded — PROGMEM Edition)
└── data/                 ← Source files (for reference only — NOT uploaded separately)
    ├── index.html        ← Chat UI structure
    ├── style.css         ← Dark theme, glassmorphism styling
    └── app.js            ← WebSocket client + encryption
```

> **Note:** The `data/` folder is for reference only. All HTML, CSS, and JavaScript are embedded directly in `meshtalk.ino` as PROGMEM strings. **No separate filesystem upload is needed.**

---

## 🛠️ Prerequisites

| Item | Details |
|------|---------|
| **Board** | ESP8266 (NodeMCU v3, Wemos D1 Mini, or any ESP-12E/F module) |
| **Arduino IDE** | Version 1.8.x or 2.x |
| **ESP8266 Board Package** | `esp8266` by ESP8266 Community (v3.1.x+) |

### Required Libraries (install via Library Manager or ZIP)

| Library | Author | Purpose |
|---------|--------|---------|
| `ESPAsyncWebServer` | me-no-dev | Async HTTP + WebSocket server |
| `ESPAsyncTCP` | me-no-dev | Async TCP backend |
| `ArduinoJson` | Benoît Blanchon | JSON parsing/serialisation |

> **No LittleFS plugin needed!** Everything is compiled into the firmware.

---

## 🚀 Setup Instructions (Step-by-Step)

### Step 1 — Install Arduino IDE & ESP8266 Board Package

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software).
2. Open **File → Preferences**.
3. In **Additional Board Manager URLs**, add:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
4. Go to **Tools → Board → Boards Manager**, search for **esp8266**, and install it.
5. Select your board: **Tools → Board → ESP8266 Boards → NodeMCU 1.0 (ESP-12E Module)**.

### Step 2 — Install Required Libraries

1. Open **Sketch → Include Library → Manage Libraries**.
2. Search and install:
   - **ArduinoJson** (by Benoît Blanchon)
3. For **ESPAsyncWebServer** and **ESPAsyncTCP** (not in Library Manager):
   - Download from GitHub:
     - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip)
     - [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP/archive/master.zip)
   - Install: **Sketch → Include Library → Add .ZIP Library** → select each downloaded ZIP.

### Step 3 — Configure Board Settings

In **Tools** menu, set:

| Setting | Value |
|---------|-------|
| Board | NodeMCU 1.0 (ESP-12E Module) |
| Flash Size | 4MB (FS: 2MB, OTA: ~1019KB) |
| Upload Speed | 115200 |
| CPU Frequency | 80 MHz |
| Port | Your ESP8266's COM port |

### Step 4 — Upload the Sketch

1. Open `meshtalk/meshtalk.ino` in Arduino IDE.
2. Click the **Upload** button (→).
3. Wait for compilation and upload to finish.
4. Open **Tools → Serial Monitor** at **115200 baud** to see startup logs.

**That's it! Just one upload — no filesystem upload needed.** ✅

---

## 📱 How to Use

1. **Power on** the ESP8266.
2. On your phone/laptop, connect to WiFi network **"MeshTalk"** (open network, no password).
3. Open a browser and go to: **http://192.168.4.1**
4. Enter a **username** and an **encryption passphrase**.
5. Share the same passphrase with all participants (verbally or via a secure channel).
6. Start chatting! 🎉

### Private Messages
- Click on a user's name in the sidebar to start a private conversation.
- Private messages are only sent to you and the selected recipient.

---

## 🔐 Security Details

| Feature | Implementation |
|---------|---------------|
| **Encryption** | RC4 stream cipher with key derivation (HTTP) / AES-256-GCM (HTTPS) |
| **Key Derivation** | FNV-1a hash with key stretching (HTTP) / PBKDF2-SHA256 100K iterations (HTTPS) |
| **Salt** | Random bytes per message |
| **IV (Nonce)** | Random bytes per message |
| **Auto-detection** | Automatically uses strongest available encryption |
| **ESP8266 Role** | Relay only — never decrypts messages |

### How Encryption Works

```
Sender                          ESP8266                        Receiver
  │                               │                               │
  │ plaintext + passphrase        │                               │
  │ ─── key derivation ──► key   │                               │
  │ ─── encrypt ──►              │                               │
  │         ciphertext + iv + salt│                               │
  │ ─────────────────────────────►│                               │
  │                               │  relay ciphertext (untouched) │
  │                               │──────────────────────────────►│
  │                               │                  passphrase + │
  │                               │         ciphertext + iv + salt│
  │                               │    ◄── key derivation ── key  │
  │                               │         ◄── decrypt ──        │
  │                               │                    plaintext  │
```

---

## ✨ Features

- 🔒 **End-to-end encryption** — ESP8266 never sees plaintext
- 📡 **Fully offline** — no internet required
- 💬 **Real-time chat** via WebSockets
- 🔔 **Typing indicators** — see who's typing in real-time
- 👤 **User presence** — online user list with avatars
- 🤫 **Private messages** — click a user to send PM
- 🎨 **Dark glassmorphism UI** — beautiful modern design
- 📱 **Responsive** — works on phones, tablets, and desktops
- ⚡ **PROGMEM Edition** — single upload, no LittleFS hassle
- 🔄 **Auto-reconnect** — reconnects with exponential backoff

---

## 🧩 Technical Architecture

```
┌─────────────────────────────────────────────────┐
│                  ESP8266 (AP Mode)               │
│                                                  │
│  WiFi AP: "MeshTalk" ──► 192.168.4.1            │
│                                                  │
│  ┌──────────────────┐  ┌──────────────────────┐ │
│  │  HTTP Server :80  │  │  WebSocket at /ws    │ │
│  │                   │  │                      │ │
│  │  Serves:          │  │  Events:             │ │
│  │  • index.html     │  │  • join / leave      │ │
│  │  • style.css      │  │  • message relay     │ │
│  │  • app.js         │  │  • typing indicator  │ │
│  │                   │  │  • user list sync    │ │
│  │  (from PROGMEM)   │  │  • ping / pong       │ │
│  └──────────────────┘  └──────────────────────┘ │
└─────────────────────────────────────────────────┘
         │                         │
    ┌────┴────┐              ┌─────┴─────┐
    │ Phone 1 │              │ Laptop 2  │
    │ Browser │◄── WiFi ────►│ Browser   │
    └─────────┘              └───────────┘
```

---

## ⚠️ Important Notes

- **Maximum 8 users** can connect simultaneously (configurable in code).
- **All users must use the same passphrase** to read each other's messages.
- The ESP8266 creates an **open WiFi network** by default. To add a WiFi password, change `AP_PASS` in the `.ino` file.
- **Range:** Typically 20–50 meters (can be extended with an external antenna).

---

## 🐛 Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't see "MeshTalk" WiFi | Check power supply; try resetting ESP8266 |
| 404 error at 192.168.4.1 | Make sure you uploaded the latest PROGMEM version of the sketch |
| "Server full" error | Max 8 users — disconnect unused clients |
| Messages show 🔒 encrypted | Users have different passphrases — agree on one |
| WebSocket won't connect | Ensure you're on the MeshTalk WiFi, not mobile data |
| Garbled text in Serial Monitor | Normal during boot — wait for clean output after `========` banner |

---

## 📄 License

This project is open source and free to use for educational purposes.
Built for college IoT + Cybersecurity projects.


Thank you!
