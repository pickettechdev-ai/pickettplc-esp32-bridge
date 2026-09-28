/*
 * PickettPLC ESP32 I/O Bridge — firmware v2.1
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
 * BOARD
 *   Pin lists are for a classic ESP32 DevKit (ESP32-WROOM-32).
 *   For other boards (S3, C3, WROVER) change IN_PINS / OUT_PINS.
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

const char* FW_VERSION = "2.1";

// ---- Inputs ---------------------------------------------------------------
// Wire each push-button or sensor contact between the pin and GND.
// INPUT_PULLUP: the pin reads LOW when pressed, which is reported as 1.
const int   IN_PINS[8] = { 13, 14, 27, 26, 25, 33, 32, 4 };
const char* IN_ADDR[8] = { "I0.0", "I0.1", "I0.2", "I0.3", "I0.4", "I0.5", "I0.6", "I0.7" };

// ---- Outputs --------------------------------------------------------------
// HIGH when the coil is energised.
// LED:   pin -> 330 ohm resistor -> LED (long leg) ... LED (short leg) -> GND
// Relay: use a relay MODULE with its own driver. Never a bare relay coil.
// Note: GPIO 16/17 are not available on WROVER boards; pick other pins there.
const int OUT_PINS[8] = { 23, 22, 21, 19, 18, 17, 16, 15 };

// The on-board LED (GPIO 2 on most DevKits) copies Q0.0, so the Blink Test
// works with no wiring at all. Set to -1 to turn this off.
const int ONBOARD_LED = 2;

const unsigned long LINK_TIMEOUT_MS = 1500;  // no updates for this long -> all outputs off
const unsigned long INPUT_REPORT_MS = 50;    // how often inputs are sent to the browser

bool outState[8];
unsigned long lastRx = 0;
unsigned long lastReport = 0;
String serialLine;

// ---------------------------------------------------------------------------
void setOutput(int i, bool on) {
  outState[i] = on;
  digitalWrite(OUT_PINS[i], on ? HIGH : LOW);
  if (i == 0 && ONBOARD_LED >= 0) digitalWrite(ONBOARD_LED, on ? HIGH : LOW);
}

void allOutputsOff() {
  for (int i = 0; i < 8; i++) setOutput(i, false);
}

String helloJson() {
  String s = "{\"hello\":\"PickettPLC-ESP32\",\"fw\":\"";
  s += FW_VERSION;
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
    s += (digitalRead(IN_PINS[i]) == LOW) ? "1" : "0";
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
  for (int i = 0; i < 8; i++) pinMode(IN_PINS[i], INPUT_PULLUP);
  for (int i = 0; i < 8; i++) pinMode(OUT_PINS[i], OUTPUT);
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
