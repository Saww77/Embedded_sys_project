# Standalone dashboard verification

- 13 state/HTTP tests passed from this folder with Python's standard library.
- JavaScript syntax check passed.
- Real Mosquitto + Chromium/Playwright integration passed with a simulated
  Pico protocol fixture. This is software integration, not physical hardware
  validation.
- The app and tests import no firmware code or files from the tools folder.
- No Pico firmware source was changed for this separated application.

Browser test results:

```text
PASS CLI: empty address reprompted; valid IP starts local HTTP dashboard; no startup command
PASS browser: retained status does not enable START; connected STOP remains available
PASS end-to-end: button -> HTTP -> real Mosquitto -> binary 01/00, QoS 0, non-retained -> fixture ACK -> browser
PASS browser: rapid START then STOP preserves command order
PASS browser: mobile layout has no horizontal overflow
PASS browser: stale heartbeat disables START and keeps STOP available
PASS browser: broker loss disables controls; no auto-command; no JavaScript errors
```
