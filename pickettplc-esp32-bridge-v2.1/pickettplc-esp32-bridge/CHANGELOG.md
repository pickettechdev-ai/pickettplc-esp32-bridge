Changelog
v2.2 · 2026-09-28
Fixed
ESP32-C3 boot loop. On an ESP32-C3 the v2.1 firmware rebooted forever (`rst:0x8 (TG1WDT_SYS_RST)` in the Serial Monitor) and PickettPLC showed "No reply. Is the PickettPLC firmware uploaded?". The classic-ESP32 pin list used GPIO 13–17, which are the flash pins on a C3. See docs/esp32-c3-boot-loop.md.
Added
Pin map chosen automatically from Tools → Board: classic ESP32 (8 in / 8 out), ESP32-C3 (up to 4 in / 8 out), ESP32-S3 (8 in / 8 out).
ESP32-C3 build uses GPIO 20/21 as inputs only when USB CDC On Boot is enabled, so boards with a USB-serial chip keep working.
ESP32-C3 SuperMini works with no wiring: the blue on-board LED (GPIO 8, active-low) copies Q0.0 and the BOOT button (GPIO 9) is I0.0.
`ONBOARD_LED_ON` sets whether the on-board LED lights on HIGH or LOW.
`-1` in `IN_PINS` / `OUT_PINS` means "no pin"; that address reads 0 or is ignored.
`hello` reply now includes `"board"` (`"ESP32"`, `"ESP32-C3"` or `"ESP32-S3"`).
Unchanged
Classic ESP32 DevKit pins, wiring, USB/Wi-Fi protocol and the 1.5 s safety timeout are the same as v2.1. Existing classic-ESP32 setups need no rewiring.
