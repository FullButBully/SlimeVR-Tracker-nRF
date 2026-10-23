# Server could see the receiver but not the tracker

Observed on 2026-10-22 with SlimeVR Server v22.0.0-rc.1, commit `7d1e1427`.
The Server was already running. No Server code/configuration was changed,
and no receiver firmware changes were made during this diagnosis.

## Evidence before the fix

- Server logged `HID device detected: C4E6674E8A2A9504` and
  `Registered HID device 99806266A844 (hidId=0)`, but its tracker list stayed empty.
- A five-second receiver capture contained 204 registration packets and zero
  quaternion/metadata packets. USB enumeration was working.
- DK RTT showed continued six-axis sampling, zero sensor packet errors and a
  valid pairing address. Sensor operation was not the missing stage.
- Live ESB state was PTX_TX (3), RADIO state TX (11), with END asserted but
  PHYEND absent. The transmitter had stalled again after the short initial
  test; a zero TX-error counter did not prove continued delivery in no-ACK mode.

## Tracker-only correction

The bench profile enables `CONFIG_ESB_CLOCK_INIT=y`, making the SDK own the
radio HF clock and nRF54L PLL/constant-latency errata workarounds for the ESB
lifetime. The application was still independently requesting/releasing the
clock and re-triggering PLLSTART per transmission.

`src/connection/esb.c` now skips the application's hardware clock operations
when SDK ESB clock initialization is enabled. The application flag remains
available to existing scheduling code; SDK `esb_init()` / `esb_disable()`
remain the sole clock owners. Builds without ESB clock initialization retain
their existing application-managed behavior.

After rebuilding and flashing the DK, the already-running, unmodified
Server logged `Tracker list size changed to 1` at 20:39:00.806.

The subsequent 120.063-second receiver capture contained 7824 USB reports,
8527 tracker packets and 6113 quaternion packets. Tracker packets continued
through the end of the capture; the largest inter-packet gap was 140 ms.
Rotation changed by almost 180 degrees during the capture, and quaternion
norms stayed within approximately 0.999947..1.000000. This is receiver-side
evidence alongside the Server's tracker-creation log; no Server restart,
source patch or agent-written settings change was required.

## Validation artifacts

- Tracker build: `build-artifacts/radio-clock-owner-build.log`.
- Tracker boot/sensor log: `build-artifacts/server-fix-tracker.log`.
- Receiver capture: sibling receiver project's
  `build-artifacts/server-fix-receiver-hid.json` and `.jsonl`.
- Previous failure capture: sibling receiver project's
  `build-artifacts/server-diagnosis-hid.json`.
- Final flashed DK HEX/ELF/map: `build-artifacts/final/`.

This corrects the observed stall and verifies Server tracker creation.
The continuous capture duration is a bounded stability test, not an
indefinite reliability or battery-life guarantee.
