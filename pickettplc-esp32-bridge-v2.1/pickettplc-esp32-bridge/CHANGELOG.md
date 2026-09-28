# Changelog

## v2.1 — 2026-09-28
First public release, for PickettPLC v2.2.

- USB connection (Web Serial) that works straight from pickettech.com in Chrome and Edge. No extra libraries needed.
- Optional Wi-Fi mode (`USE_WIFI 1`) using the WebSockets library by Markus Sattler.
- 8 inputs (I0.0–I0.7) with internal pull-ups and 8 outputs (Q0.0–Q0.7).
- Q0.0 mirrored on the on-board LED so the Blink Test works with no wiring.
- Safety timeout: all outputs switch off if the browser stops sending for 1.5 s.
- Line-based JSON protocol with a hello handshake and firmware version report.
