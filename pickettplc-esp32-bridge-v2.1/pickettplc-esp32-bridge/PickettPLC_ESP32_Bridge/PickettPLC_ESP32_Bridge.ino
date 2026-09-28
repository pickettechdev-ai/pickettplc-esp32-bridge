/*
 * PickettPLC ESP32 I/O Bridge — firmware v2.2
 * PICKETTECH · https://pickettech.com/PickettPLC
 * Source & releases: https://github.com/Pickettechdev-ai/pickettplc-esp32-bridge
 *
 * Turns an ESP32 into real I/O for the PickettPLC browser simulator.
 * Your ladder logic runs in the browser. The ESP32 reads 8 inputs and
 * drives 8 outputs, so real buttons and LEDs/relays follow the program.
 *
 * TWO WAYS TO CONNECT
 *   USB (default)  Works straight from pickettech.com in Chrome or Edge on a
 *                  computer. No extra libraries needed.
 *   Wi-Fi (option) Set USE_WIFI to 1 below and fill in your Wi-Fi details.
 *                  Needs the "WebSockets" library by Markus Sattler.
 *                  Browsers block Wi-Fi connections from the https website,
 *                  so use it with a saved copy of the page (see the guide).
 *
 * BOARD (picked automatically from Tools > Board in the Arduino IDE)
 *   ESP32 (classic DevKit, WROOM-32)  8 inputs / 8 outputs
 *   ESP32-C3 (DevKitM-1, SuperMini)   5 inputs / 8 outputs
 *   ESP32-S3 (DevKitC-1)              8 inputs / 8 outputs
 *   Each chip has different pins. Using the wrong list can make the board
 *   reboot forever (see docs/esp32-c3-boot-loop.md), so leave the lists
 *   below alone unless you know your board's pinout.
 *
 * ESP32-C3 / ESP32-S3 USERS
 *   Set Tools > USB CDC On Boot > Enabled if your board plugs in through
 *   the chip's own USB (no separate USB-serial chip). Otherwise the
 *   browser gets no reply.
 *
 * SAFETY
 *   Training use only. Outputs switch OFF if the browser stops sending
 *   updates for 1.5 s. Never use this for safety functions or E-stops.
 */

#define USE_WIFI 0   // 0 = USB only, 1 = USB + Wi-Fi

#if USE_WIFI
  #include <WiFi.h>
  #include <WebSocketsServer.h>
  const char* WIFI_SSID     = "YOUR_WIFI_NAME";
  const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
  WebSocketsServer ws(81);            // browser connects to ws://<ip>:81/plc
  int wsClient = -1;
#endif

const char* FW_VERSION = "2.2";

// ---- Pin maps ---------------------------------------------------------------
// Inputs:  wire each push-button or sensor contact between the pin and GND.
//          INPUT_PULLUP: the pin reads LOW when pressed, which is reported as 1.
// Outputs: HIGH when the coil is energised.
//          LED:   pin -> 330 ohm resistor -> LED (long leg) ... LED (short leg) -> GND
//          Relay: use a relay MODULE with its own driver. Never a bare relay coil.
// -1 means "no pin". That address always reads 0 / is ignored.

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3. GPIO 12-17 run the flash chip: never use them.
  // GPIO 18/19 are the USB data lines. GPIO 2, 8 and 9 are boot (strapping)
  // pins, so they are inputs here: an LED to GND on them can stop the boot.
  const char* BOARD_NAME = "ESP32-C3";
  #if ARDUINO_USB_CDC_ON_BOOT
    // Native USB: GPIO 20/21 (UART0) are free
    const int IN_PINS[8]  = { 2, 8, 9, 20, 21, -1, -1, -1 };
  #else
    // USB-serial chip on UART0: GPIO 20/21 carry Serial, so leave them alone
    const int IN_PINS[8]  = { 2, 8, 9, -1, -1, -1, -1, -1 };
  #endif
  const int OUT_PINS[8]  = { 0, 1, 3, 4, 5, 6, 7, 10 };
  const int ONBOARD_LED  = -1;   // SuperMini LED is GPIO 8, used as an input above

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3. GPIO 26-32 run the flash (33-37 too on octal-PSRAM modules).
  // GPIO 19/20 are USB. GPIO 0, 3, 45, 46 are strapping pins.
  const char* BOARD_NAME = "ESP32-S3";
  const int IN_PINS[8]   = { 4, 5, 6, 7, 15, 16, 17, 18 };
  const int OUT_PINS[8]  = { 8, 9, 10, 11, 12, 13, 14, 21 };
  const int ONBOARD_LED  = -1;   // S3 DevKit LED is an addressable RGB LED

#else
  // Classic ESP32 DevKit (ESP32-WROOM-32).
  // GPIO 6-11 run the flash: never use them.
  // Note: GPIO 16/17 are not available on WROVER boards; pick other pins there.
  const char* BOARD_NAME = "ESP32";
  const int IN_PINS[8]   = { 13, 14, 27, 26, 25, 33, 32, 4 };
  const int OUT_PINS[8]  = { 23, 22, 21, 19, 18, 17, 16, 15 };
  // The on-board LED (GPIO 2 on most DevKits) copies Q0.0, so the Blink Test
  // works with no wiring at all. Set to -1 to turn this off.
  const int ONBOARD_LED  = 2;
#endif

const char* IN_ADDR[8] = { "I0.0", "I0.1", "I0.2", "I0.3", "I0.4", "I0.5", "I0.6", "I0.7" };

const unsigned long LINK_TIMEOUT_MS = 1500;  // no updates for this long -> all outputs off
const unsigned long INPUT_REPORT_MS = 50;    // how often inputs are sent to the browser

bool outState[8];
unsigned long lastRx = 0;
unsigned long lastReport = 0;
String serialLine;

// ---------------------------------------------------------------------------
void setOutput(int i, bool on) {
  outState[i] = on;
  if (OUT_PINS[i] >= 0) digitalWrite(OUT_PINS[i], on ? HIGH : LOW);
  if (i == 0 && ONBOARD_LED >= 0) digitalWrite(ONBOARD_LED, on ? HIGH : LOW);
}

void allOutputsOff() {
  for (int i = 0; i < 8; i++) setOutput(i, false);
}

String helloJson() {
  String s = "{\"hello\":\"PickettPLC-ESP32\",\"fw\":\"";
  s += FW_VERSION;
  s += "\",\"board\":\"";
  s += BOARD_NAME;
  s += "\",\"inputs\":8,\"outputs\":8}";
  return s;
}

String inputsJson() {
  String s = "{\"inputs\":{";
  for (int i = 0; i < 8; i++) {
    if (i) s += ",";
    s += "\"";
    s += IN_ADDR[i];
    s += "\":";
    s += (IN_PINS[i] >= 0 && digitalRead(IN_PINS[i]) == LOW) ? "1" : "0";
  }
  s += "}}";
  return s;
}

void sendLine(String msg, bool toSerial) {
  if (toSerial) { Serial.println(msg); return; }
#if USE_WIFI
  if (wsClient >= 0) ws.sendTXT(wsClient, msg);
#endif
}

// Handles one message from the browser, for example:
//   {"hello":1}
//   {"outputs":{"Q0.0":1,"Q0.1":0}}
void handleMessage(const String& msg, bool fromSerial) {
  if (msg.indexOf("\"hello\"") >= 0) sendLine(helloJson(), fromSerial);
  if (msg.indexOf("\"outputs\"") < 0) return;
  lastRx = millis();
  for (int i = 0; i < 8; i++) {
    String key = "\"Q0.";
    key += i;
    key += "\":";
    int p = msg.indexOf(key);
    if (p >= 0) setOutput(i, msg.charAt(p + key.length()) == '1');
  }
}

#if USE_WIFI
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    wsClient = num;
    sendLine(helloJson(), false);
  } else if (type == WStype_DISCONNECTED) {
    if (num == wsClient) { wsClient = -1; allOutputsOff(); }
  } else if (type == WStype_TEXT) {
    String msg;
    msg.reserve(length);
    for (size_t i = 0; i < length; i++) msg += (char)payload[i];
    handleMessage(msg, false);
  }
}
#endif

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 8; i++) if (IN_PINS[i]  >= 0) pinMode(IN_PINS[i], INPUT_PULLUP);
  for (int i = 0; i < 8; i++) if (OUT_PINS[i] >= 0) pinMode(OUT_PINS[i], OUTPUT);
  if (ONBOARD_LED >= 0) pinMode(ONBOARD_LED, OUTPUT);
  allOutputsOff();
  delay(200);
  Serial.println(helloJson());

#if USE_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("{\"log\":\"Connecting to Wi-Fi...\"}");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    ws.begin();
    ws.onEvent(onWsEvent);
    Serial.print("{\"log\":\"Wi-Fi connected. Enter this IP in PickettPLC: ");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  } else {
    Serial.println("{\"log\":\"Wi-Fi failed. Check the name and password. USB still works.\"}");
  }
#endif
}

void loop() {
  // USB: one JSON message per line
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') { handleMessage(serialLine, true); serialLine = ""; }
    else if (c != '\r' && serialLine.length() < 512) serialLine += c;
  }

#if USE_WIFI
  ws.loop();
#endif

  unsigned long now = millis();

  // Report inputs
  if (now - lastReport >= INPUT_REPORT_MS) {
    lastReport = now;
    String j = inputsJson();
    Serial.println(j);
#if USE_WIFI
    if (wsClient >= 0) ws.sendTXT(wsClient, j);
#endif
  }

  // Safety: browser gone quiet -> outputs off
  if (lastRx != 0 && now - lastRx > LINK_TIMEOUT_MS) {
    allOutputsOff();
    lastRx = 0;
  }
}
