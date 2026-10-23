# P1.13 manual System OFF and wake validation

Tested on the user's custom nRF54L15 board through J-Link PLUS 602712071.
P1.13 (DK sw0 / BUTTON 0) is internally pulled up; switch to GND to press.
SPI remains on P1.04/05/06/07 and INT1 on P1.11.

Build: `tools/build-dk.ps1 -App tracker -Wiring hardware-p1 -ButtonSleep`.
This appends button-sleep.conf to tracker.conf, uses a separate
build-tracker-hardware-p1-button-sleep directory, enables USER_SHUTDOWN,
keeps IGNORE_RESET enabled and multiple-click extra actions disabled.
The matched P1/P2 current-comparison binaries are preserved unchanged.

Changes made for this test:

- Correct CS GPIO disconnection to include the controller port. P1.07
  must use encoded GPIO 39, not raw pin 7 (which addresses P0.07).
- Flush deferred logs before System OFF, including the RTT backend.
- Log raw RESETREAS, current button level, and shutdown enable at boot.
  The logged user_shutdown=4 is a nonzero configuration bit mask (enabled).
- Add optional periodic RTT reattachment to probe.py. It follows target
  resets while SWD stays connected, but does not reconnect a lost J-Link.

The user tested long press, release, wait, then short press to wake.
`build-artifacts/button-sleep-complete-rtt.log` contains:

1. At 00:04:25.539: Button pressed.
2. At 00:04:28.878: User shutdown requested (after button release).
3. At 00:04:28.900: System off requested.
4. Configured sw0 sense, disconnected IMU wake interrupt and CS GPIO 39.
5. At 00:04:28.902: Powering off nRF.

SWD connectivity was then lost; the capture tool exited with a J-Link
connection-lost exception. That exception alone is not a firmware fault
or proof of achieved low current. Reattaching after the user's wake
operation yielded `build-artifacts/button-sleep-wake-check-rtt.log`:

- Boot reset reason 0x00000100, button=1. In the installed Nordic MDK
  nrf54l15_types.h, RESET_RESETREAS_OFF_Pos is 8: this is System OFF wake.
- Tracker ID 1, receiver D6C36476B1ED retained, no new pairing requested.
- LSM6DSV rediscovered, VQF resumed, 6947 samples per sensor at the last
  health report, zero packet errors.

This confirms manual System OFF entry and wake functionality for the
tested cycle. The user did not observe current, so neither off-state
current nor energy savings have been measured. End J-Link/RTT sessions,
power-cycle, then repeat the button action while measuring current.
Debug connectivity can affect low-power behavior.

Operation: while running, hold for over one second and release to shut
down. While off, briefly press (roughly 0.2 s) and release to wake.
Holding the wake button for over a second invokes pairing behavior.
IMU motion wake and automatic idle shutdown remain disabled.

## Official DK preparation for current measurement (2026-10-23)

The same latest image, SHA256
46695c3c4960c916db14e4b70800568879c1fc66efe0699f506ce2af19fab60b,
was flashed to the official DK through its onboard J-Link 1057787409.
`build-artifacts/dk-button-sleep-latest-rtt.log` confirms user shutdown
enabled, tracker ID 0 retained, VDD approximately 1789 mV, LSM6DSV found,
2 MHz P1 SPI, +/-16 g and +/-4000 dps, and 1152 samples per sensor with
zero packet errors by the ten-second health report. The bounded probe
session completed and closed. The user will power-cycle and enter System
OFF with BUTTON 0 / P1.13, then measure current without an active debugger.
No standby-current result has been recorded yet.
