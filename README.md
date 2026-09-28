# PickettPLC ESP32 I/O Bridge

Turn an ESP32 into real inputs and outputs for **[PickettPLC](https://pickettech.com/PickettPLC)**, the free browser-based ladder logic simulator from PICKETTECH.

Your ladder program runs in the browser. The ESP32 reads 8 real inputs (push-buttons, switches, sensor contacts) and drives 8 real outputs (LEDs, relay modules), so the program you build on screen controls real hardware.

**Firmware version:** 2.1 · **Board:** classic ESP32 DevKit (ESP32-WROOM-32)

---

## Quick start (USB)

1. **Install the Arduino IDE 2** from [arduino.cc/en/software](https://www.arduino.cc/en/software).
2. **Add ESP32 support.** File → Preferences → *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   Then Tools → Board → Boards Manager → search **esp32** (by Espressif Systems) → Install.
3. **Open the sketch.** Download this repo (green **Code** button → *Download ZIP*, or grab the latest [release](../../releases/latest)) and open `PickettPLC_ESP32_Bridge/PickettPLC_ESP32_Bridge.ino`.
4. **Upload.** Tools → Board → **ESP32 Dev Module**, choose your port, press **Upload**. If it sticks on "Connecting…", hold the **BOOT** button until it starts.
5. **Close the Serial Monitor.** Only one program can use the USB port at a time.
6. **Connect.** Open [pickettech.com/PickettPLC](https://pickettech.com/PickettPLC) in **Chrome or Edge** on a computer. In the ESP32 bar, leave **USB** selected and press **⚡ CONNECT**, then pick your board (often *CP210x* or *USB-SERIAL CH340*).
7. **Test it.** Press **💡 BLINK TEST**, then **▶ RUN**. The on-board LED flashes once a second.

USB mode needs **no extra libraries**.

> Phones, Safari and Firefox don't support Web Serial, so USB mode needs Chrome or Edge on Windows, macOS, Linux or ChromeOS.

---

## Pin map

| Address | ESP32 pin | Suggested use |
|---|---|---|
| I0.0 | GPIO 13 | Start button |
| I0.1 | GPIO 14 | Stop button |
| I0.2 | GPIO 27 | Sensor |
| I0.3 | GPIO 26 | Sensor 2 |
| I0.4 | GPIO 25 | Spare / Jog |
| I0.5 | GPIO 33 | Spare |
| I0.6 | GPIO 32 | Spare |
| I0.7 | GPIO 4 | Spare |
| Q0.0 | GPIO 23 | Motor / LED (also mirrored on the on-board LED, GPIO 2) |
| Q0.1 | GPIO 22 | Lamp |
| Q0.2 | GPIO 21 | Lamp |
| Q0.3 | GPIO 19 | Spare |
| Q0.4 | GPIO 18 | Spare |
| Q0.5 | GPIO 17 | Spare (not available on WROVER boards) |
| Q0.6 | GPIO 16 | Spare (not available on WROVER boards) |
| Q0.7 | GPIO 15 | Spare |

For other boards (ESP32-S3, C3, WROVER), edit `IN_PINS` and `OUT_PINS` at the top of the sketch.

### Wiring

- **Push-button:** one leg to the GPIO, the other to **GND**. The firmware uses the internal pull-up, so *pressed = 1* in PickettPLC.
- **LED:** GPIO → **330 Ω** resistor → LED long leg; LED short leg → GND.
- **Relay:** use a relay **module** (with its own transistor or opto driver and its own supply). Never drive a bare relay coil from a GPIO.
- ESP32 pins are **3.3 V only**. Never connect 5 V, 12 V or 24 V signals directly. For 24 V industrial sensors, use an optocoupler input board.

---

## Wi-Fi mode (optional)

pickettech.com is served over https, and browsers don't allow a secure page to open plain `ws://` connections to devices on your network. So Wi-Fi mode only works from a copy of the PickettPLC page on your own computer.

1. Install the **WebSockets** library by **Markus Sattler** (Sketch → Include Library → Manage Libraries).
2. In the sketch set `#define USE_WIFI 1` and fill in `WIFI_SSID` and `WIFI_PASSWORD`. Upload.
3. Open the Serial Monitor at **115200**; it prints the board's IP address. Then close the monitor.
4. Save the PickettPLC page to your computer (Ctrl+S / Cmd+S, *Webpage, Complete*) and open the saved file, or serve the folder locally (for example `python -m http.server`) and open `http://localhost:8000`.
5. Choose **WI-FI** in the ESP32 bar, enter the IP, leave port **81**, and press **⚡ CONNECT**.

---

## How it works

Messages are one JSON object per line (USB) or per WebSocket message (Wi-Fi):

| Direction | Message | How often |
|---|---|---|
| Browser → ESP32 | `{"hello":1}` | While connecting |
| Browser → ESP32 | `{"outputs":{"Q0.0":1,"Q0.1":0,...}}` | Every 100 ms (also the heartbeat) |
| ESP32 → Browser | `{"hello":"PickettPLC-ESP32","fw":"2.1","inputs":8,"outputs":8}` | On boot and in reply to hello |
| ESP32 → Browser | `{"inputs":{"I0.0":0,...,"I0.7":0}}` | Every 50 ms |
| ESP32 → Browser | `{"log":"..."}` | Status messages (Wi-Fi setup) |

**Safety timeout:** if no output message arrives for 1.5 s, every output switches off.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| No CONNECT pop-up | Use Chrome or Edge on a computer. |
| Port not listed | Install the CP210x or CH340 USB driver; try a data cable (some are charge-only) and another USB socket. |
| "Port is busy" | Close the Arduino Serial Monitor, Serial Plotter or any other serial app. |
| "No reply" | The PickettPLC firmware isn't on the board, or the baud rate isn't 115200. Re-upload. |
| Outputs flicker off | The browser tab went into the background, or the safety timeout tripped. Keep the page visible. |
| Input always reads 1 | The button is wired to 3.3 V instead of GND, or the pin is shorted to GND. |
| Board resets when connecting | Normal on many DevKits; it reboots in about a second and then connects. |

---

## ⚠ Safety

This is a **training tool**. Don't use it to switch mains voltage or to control real machinery, and never use it for an emergency stop or any other safety function. Real safety circuits must be hardwired through a safety relay or a certified safety controller.

---

## Licence

MIT © 2026 PICKETTECH. See [LICENSE](LICENSE).

Made by [PICKETTECH](https://pickettech.com) · Free electronics and manufacturing tools · YouTube [@pickettech](https://www.youtube.com/@pickettech) · X [@pickettechdev](https://x.com/pickettechdev)
