ESP32-C3 boot loop: "No reply. Is the PickettPLC firmware uploaded?"
PickettPLC ESP32 I/O Bridge · troubleshooting note · applies to firmware v2.1 and earlier · fixed in v2.2
What you see
In PickettPLC the ESP32 bar says Connected. Waiting for the PickettPLC firmware… and then No reply. Is the PickettPLC firmware uploaded?, even though the upload finished without errors.
Open the Arduino IDE Serial Monitor at 115200 baud and the board is rebooting over and over:
```
rst:0x8 (TG1WDT_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
Saved PC:0x40380104
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5820,len:0x110c
load:0x403cbf10,len:0xb54
load:0x403ce710,len:0x2f8c
entry 0x403cbf10
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x8 (TG1WDT_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
...
```
`TG1WDT_SYS_RST` is a watchdog reset. The chip starts, hangs, the watchdog restarts it, and it never lives long enough to send its `hello` message to the browser. The browser side is working fine; it simply gets no answer.
Why it happens
Firmware v2.1 and earlier had one pin list, written for the classic ESP32 DevKit (ESP32-WROOM-32):
```cpp
const int IN_PINS[8]  = { 13, 14, 27, 26, 25, 33, 32, 4 };
const int OUT_PINS[8] = { 23, 22, 21, 19, 18, 17, 16, 15 };
```
The ESP32-C3 is a different chip with a different pinout. On the C3, GPIO 12 to 17 are wired to the SPI flash chip that holds your program. When `setup()` runs
```cpp
for (int i = 0; i < 8; i++) pinMode(IN_PINS[i], INPUT_PULLUP);  // 13, 14 ...
for (int i = 0; i < 8; i++) pinMode(OUT_PINS[i], OUTPUT);       // ... 17, 16, 15
```
it takes those pins away from the flash. The CPU can no longer read its own code, it stalls, the watchdog fires and the board reboots, forever. Several of the other pins in the list (22, 23, 25, 26, 27, 32, 33) do not exist on a C3 at all.
The same kind of problem can hit an ESP32-S3, where GPIO 26 to 32 (and 33 to 37 on octal-PSRAM modules) belong to the flash.
The fix: firmware v2.2
Download firmware v2.2 from this repository and upload it. It picks the right pin list for the chip you select under Tools → Board in the Arduino IDE, so there is nothing to edit:
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3. GPIO 12-17 run the flash chip: never use them.
  const char* BOARD_NAME = "ESP32-C3";
  #if ARDUINO_USB_CDC_ON_BOOT
    const int IN_PINS[8]  = { 9, 2, 20, 21, -1, -1, -1, -1 };
  #else
    const int IN_PINS[8]  = { 9, 2, -1, -1, -1, -1, -1, -1 };
  #endif
  const int OUT_PINS[8]    = { 0, 1, 3, 4, 5, 6, 7, 10 };
  const int ONBOARD_LED    = 8;     // SuperMini blue LED
  const int ONBOARD_LED_ON = LOW;   // it lights when the pin is LOW

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  const char* BOARD_NAME = "ESP32-S3";
  const int IN_PINS[8]   = { 4, 5, 6, 7, 15, 16, 17, 18 };
  const int OUT_PINS[8]  = { 8, 9, 10, 11, 12, 13, 14, 21 };
  const int ONBOARD_LED    = -1;
  const int ONBOARD_LED_ON = HIGH;

#else
  // Classic ESP32 DevKit: unchanged from v2.1
  const char* BOARD_NAME = "ESP32";
  const int IN_PINS[8]   = { 13, 14, 27, 26, 25, 33, 32, 4 };
  const int OUT_PINS[8]  = { 23, 22, 21, 19, 18, 17, 16, 15 };
  const int ONBOARD_LED    = 2;
  const int ONBOARD_LED_ON = HIGH;
#endif
```
`-1` means "no pin here". v2.2 skips those addresses everywhere a pin is touched, so an unused input always reads 0 and an unused output is ignored:
```cpp
for (int i = 0; i < 8; i++) if (IN_PINS[i]  >= 0) pinMode(IN_PINS[i], INPUT_PULLUP);
for (int i = 0; i < 8; i++) if (OUT_PINS[i] >= 0) pinMode(OUT_PINS[i], OUTPUT);

// writing an output
if (OUT_PINS[i] >= 0) digitalWrite(OUT_PINS[i], on ? HIGH : LOW);

// reading an input
s += (IN_PINS[i] >= 0 && digitalRead(IN_PINS[i]) == LOW) ? "1" : "0";
```
The `hello` reply now also says which pin map was built in, so you can check it in the Serial Monitor:
```json
{"hello":"PickettPLC-ESP32","fw":"2.2","board":"ESP32-C3","inputs":8,"outputs":8}
```
ESP32-C3 wiring (v2.2)
The C3 has far fewer GPIOs than a classic ESP32, so it gets 8 outputs and up to 4 inputs. Two things work with no wiring at all on a C3 SuperMini: the blue on-board LED copies Q0.0, and the BOOT button is I0.0.
PickettPLC	C3 GPIO	Notes
Q0.0	0	also lights the on-board LED (GPIO 8)
Q0.1	1	
Q0.2	3	
Q0.3	4	
Q0.4	5	
Q0.5	6	
Q0.6	7	
Q0.7	10	
I0.0	9	the BOOT button; don't hold it while powering up
I0.1	2	boot pin: don't hold this button down while powering up
I0.2	20	only with USB CDC On Boot: Enabled
I0.3	21	only with USB CDC On Boot: Enabled
I0.4 – I0.7	—	not available on the C3
Quick test with nothing wired: upload v2.2, connect PickettPLC, press 💡 BLINK TEST then ▶ RUN, and the blue LED flashes once a second. Press BOOT and I0.0 lights in the I/O table.
The on-board LED is on GPIO 8 and is wired to 3.3 V, so it lights when the pin is LOW. v2.2 handles that with `ONBOARD_LED_ON = LOW`; set `ONBOARD_LED` to `-1` if your board has no LED there.
Why the boot pins (2, 8, 9) never drive an external LED: they decide how the C3 starts up. An LED and resistor to GND on one of them can pull it low at power-up and stop the board from booting. A push-button to GND is fine as long as it isn't pressed during power-up.
Why GPIO 18 and 19 are not used: they are the USB data lines on the C3.
Wiring is the same as on the classic board. Buttons go between the pin and GND (the firmware uses the internal pull-up). LEDs go pin → 330 Ω resistor → LED → GND. For relays, use a relay module with its own driver, never a bare relay coil.
Arduino IDE settings for the C3
Tools → Board → ESP32C3 Dev Module (or your board's own entry).
Tools → USB CDC On Boot → Enabled if your board plugs in through the C3's own USB, with no separate USB-serial chip (most SuperMini and similar small boards). If this is left disabled, `Serial` is sent out on GPIO 20/21 instead of the USB port and PickettPLC gets no reply.
Boards with a separate USB-serial chip (a CP2102 or CH340 near the USB socket, like the ESP32-C3-DevKitM-1) should leave it Disabled. v2.2 detects this and keeps GPIO 20/21 free for `Serial`.
Upload.
Check it worked
Open the Serial Monitor at 115200 baud and press the board's RESET button. You should see the boot text once, then the `hello` line above, then a steady stream of `{"inputs":{...}}` lines. No repeating `rst:0x8`.
Close the Serial Monitor. Only one program can use the USB port at a time; if the Arduino IDE still has it open, PickettPLC cannot connect.
In PickettPLC (Chrome or Edge), press ⚡ CONNECT and pick the port.
Still boot-looping?
Some low-cost C3 boards need Tools → Flash Mode → DIO and Flash Frequency → 40 MHz. If the loop continues on v2.2 with those settings, open an issue with your board's name, a photo of the board and the Serial Monitor output.
