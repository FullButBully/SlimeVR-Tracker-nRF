# nRF54L15 DK + LSM6DSV validation

## Repository state (2026-10-23)

The current source preserves the user's rollback: LSM6DSV shutdown uses
the original software reset followed by a 1 ms delay. The experimental
ODR/FIFO shutdown readback and automatic return-to-OFF on debug wake
are not included. RTT console polling, blocking RTT logging, and periodic
sensor diagnostics remain enabled in the bench profile.

Build the current P1/button profile with:

```powershell
./tools/build-dk.ps1 -App tracker -Wiring hardware-p1 -ButtonSleep
```

Use `-Wiring hardware` for the P2/SPIM00 profile or `-Wiring software`
for the original bit-banged wiring. Pin mappings are in hardware-p1.overlay,
hardware.overlay, and wiring.overlay. The optional `-CurrentMeasurement`
profile currently inherits the original console/diagnostic settings as
requested; it does not disable logging or reduce radio output power.

Manual System OFF and wake have been observed, but repeated-cycle standby
current remains unresolved. Do not treat these bench settings as a verified
low-power production profile. See SECOND-SLEEP-FIX.md for the failed attempts
and rollback, and SPI-POWER-COMPARISON.md for the P1/P2 setup.

Build directories, firmware artifacts, raw RTT/HID logs, and downloaded tools
are local validation outputs and are excluded from Git. Paths to those files
in the historical records below refer to the original test workspace.

## Current maximum-range profile (2026-10-22)

### Startup discovery correction

The user subsequently reported a failure after restarting the tracker.
`build-artifacts/reboot-failure-rtt.log` shows both boot-time discovery
attempts finishing within 17 ms with all-zero SPI responses and no IMU.
The radio still delivered metadata/status packets and Server registered
one tracker, but there was no sensor fusion. A later console `scan`
successfully read WHO_AM_I=0x70 and restored LSM6DSV/VQF operation.

The first correction, a 100 ms discovery delay, passed software reboot
but FAILED the user's actual power-cycle test: both scans at 110/116 ms
returned zero. Evidence: `build-artifacts/cold-boot-fixed-rtt.log`.
The final profile retains the 100 ms delay and enables up to 20 discovery
retries spaced 250 ms apart. Other boards retain defaults of zero startup
delay and one retry after 5 ms.

The user performed another real power cycle, waited six seconds, and moved
the IMU. Without an active J-Link session during that boot, discovery first
failed at 110 ms, then succeeded at 361 ms on retry 1/20. Sampling continued
through 35 seconds with 4032 samples per sensor and zero packet errors.
Evidence: `build-artifacts/cold-retry-boot-rtt.log`. The simultaneous receiver
capture (`../SlimeVR-Tracker-nRF-Receiver/build-artifacts/cold-retry-hid.json`)
contains 1688 quaternion packets over 45.078 seconds, with a 67.16 degree
orientation change. This establishes recovery for the tested real cold boot;
the physical reason for the initial all-zero SPI response is not yet isolated.
No receiver firmware or Server code/config changes were needed.
The prior maximum-range image is saved under
`build-artifacts/max-range-before-boot-fix`; `build-artifacts/final`
contains the startup correction with the same maximum measurement ranges.

At the user's request, `tracker.conf` now selects accelerometer +/-16 g and
gyroscope +/-4000 dps, the LSM6DSV's maximum ranges. The flashed build's RTT
log (`build-artifacts/max-range-rtt.log`) confirms both ranges, 2 MHz SPI,
116.88 Hz sampling, and 2910 samples per sensor with zero packet errors
in the captured 25-second interval. Raw outputs remain 16-bit, with nominal
steps of 0.488 mg/LSB and 0.140 dps/LSB respectively.

A subsequent 15-second receiver capture (`../SlimeVR-Tracker-nRF-Receiver/
build-artifacts/max-range-hid.json`) contains 148 metadata packets and 133
type-6 status packets from tracker 99806266A844. No type-1/2/4 quaternion
packets occurred in this stationary capture; a new motion test was not run.
The earlier Server-fix image is preserved in `build-artifacts/server-fix-final`;
`build-artifacts/final` now contains the flashed maximum-range image.

Current and energy consumption have not been measured. The DK voltage ADC
cannot supply a current measurement. Nordic documents external PPK2,
ammeter, or oscilloscope measurements through the VDD:nRF P6 connector:
https://docs.nordicsemi.com/r/bundle/ug_nrf54l15_dk/page/ug/nrf54l15_dk/measurements/current_measurements.html
The connected-device inventory did not identify a PPK2. This bench profile
keeps sensors and radio active and enables logging; it is not a measured
battery-life profile. The IMU supply must be included downstream of the
measurement point to measure the complete tracker rather than only the SoC.

Test environment: NCS 3.4.1 / Zephyr 4.4.2, J-Link 9.82, DK probe
1057787409. DK/IMU supply is 1.8 V according to the operator. The probe
reports VTref=3.3 V; that reading is not an independent measurement of VDDIO.
DK external memory is disconnected.

## Current hardware SPI wiring

| LSM6DSV signal | DK pin |
| --- | --- |
| SCK / SPC | P2.06 |
| SDI (sensor input / master MOSI) | P2.08 |
| SDO (sensor output / master MISO) | P2.04 |
| CS, active low | P2.10 |
| INT1, active low / open drain | P1.11 |
| Supply, VDDIO | 1.8 V |
| GND | DK GND |

`hardware.overlay` contains the changeable hardware pin configuration. Change
its default AND sleep pinctrl groups together, plus `cs-gpios`/`int0-gpios`
as needed. Hardware SPI pins must satisfy Nordic's peripheral pin assignment
table; arbitrary GPIO routing is not supported. Keep the sensor's maximum
SPI clock below 10 MHz. This profile uses mode 3 at 2 MHz: SPIM00's 128 MHz
clock and maximum divisor of 126 make exactly 1 MHz invalid in this SDK.

The original SDI=P2.04, SDO=P2.08, INT1=P2.03 wiring is retained in
`wiring.overlay`. It requires software SPI. GPIO2 does not have a GPIOTE
instance in this DK description, so the tracker falls back to timed FIFO
polling when GPIO interrupt setup returns `-ENOTSUP`.

## Build and flash

Run from the repository root in PowerShell. SDK/toolchain paths are localized
in `tools/ncs-env.ps1`. The default profile is the CURRENT hardware wiring.

```powershell
.\tools\build-dk.ps1 -App diagnostic -Wiring hardware
. .\tools\ncs-env.ps1
python tools/probe.py --hex build-diagnostic-hardware/zephyr/zephyr.hex --log build-artifacts/diagnostic-hw-rtt.log --seconds 20

.\tools\build-dk.ps1 -App tracker -Wiring hardware
. .\tools\ncs-env.ps1
python tools/probe.py --hex build-tracker-hardware/zephyr/zephyr.hex --log build-artifacts/tracker-hw-rtt.log --seconds 30
```

Use `-Wiring software` only with the ORIGINAL wiring. Output directories end
in `-software` for that profile. Flashing replaces the application image.
The first attempted original-image backup failed and is not available.

Capture an already programmed image with its exact map file:

```powershell
python tools/probe.py --map build-tracker-hardware/zephyr/zephyr.map --log build-artifacts/tracker-followup.log --seconds 30
```

Use only one debugger/programmer at a time. The capture tool derives the RTT
control-block address from the linker map; automatic scanning can find a
stale control block left by another image. Buffered lines can predate the
latest boot banner. J-Link connection/reset can restart the MCU, so capture
is not a passive measurement of uninterrupted uptime.

## Bench configuration and fixes

`tracker.conf` disables automatic shutdown, IMU sleep and magnetometer use,
delays sensor low power for ten minutes, and enables health logs every five
seconds. It requests 100 Hz per sensor (the LSM6DSV selects its supported
120 Hz rate). USB HID is unavailable on nRF54L15; ESB is used for receiver
communication. This configuration does not validate radio/receiver delivery
by itself.

Changes made while bringing up this hardware:

- Respect the overlay SPI frequency limit during scan and driver setup.
- Check GPIO setup errors and fall back to timed FIFO polling for unsupported
  interrupt GPIOs.
- Clamp FIFO batching interrupt thresholds to at least one sample. At
  120 Hz, a 6 ms interval previously truncated to zero.
- Provide configurable extra stack space for application threads. A fatal
  breakpoint identified `temp_thread_id`, with an original 256-byte stack,
  as the first stack-check failure on this SDK. The bench profile adds
  1024 bytes per application thread; other profiles default to zero extra.
- Skip the external magnetometer scan when magnetometer use is disabled.
- Include the SDK-version header so ADC setup selects the correct API, use
  symbolic ADC input identifiers, and measure DK VDD directly with a supported
  nRF54L gain. nRF54L15 has no VDDH/5 input. Disable the redundant Zephyr
  voltage-divider sensor because the application already performs ADC sampling.
  Initialize failed voltage-read outputs and propagate the sampling error.

## Evidence

`build-artifacts/diagnostic-hw-rtt.log` records 32/32 WHO_AM_I reads equal to
0x70, successful configuration readbacks, about 117 six-axis samples per
second, zero SPI errors, and INT1 counts increasing at the sample rate.
Static acceleration magnitude is approximately 1 g.

ADC validation with info logging measured approximately 1813 mV on DK VDD,
consistent with the 1.8 V supply. The final profile logs ADC errors only.

`build-artifacts/tracker-hw-rtt.log` contains the latest full tracker run.
Check its boot banner, `Found LSM6DSV`, `Initialized fusion`, and the
`IMU health` lines for increasing accel/gyro counts, zero packet errors,
`irq=1`, finite normalized quaternions and plausible temperature.

Final captured run: approximately 50 seconds, 5820 accelerometer samples and
5820 gyroscope samples; zero packet errors, communication errors, interrupt
timeouts or fatal errors. Configured sensor rate after oscillator correction
is 116.88 Hz. Quaternion norms span 0.9999953 to 1.0000067. Final DK VDD
reading is 1816 mV. A changed static orientation was observed after the
operator moved the IMU, then remained stable; the motion trajectory itself
was not continuously recorded.

One startup SOC-learning warning follows the previous invalid voltage data.
It resets the estimator and does not recur in the captured stable run. The
DK's reported 100% uses the no-mapping bench setting; it is not a measured
lithium-battery state of charge.

The exact flashed HEX/ELF/map are copied to `build-artifacts/final/`, with
SHA-256 hashes and measured results in `build-artifacts/validation-summary.json`.

## Receiver link update

The receiver was subsequently updated from the official repository to commit
`e3d66d5dbf70`, and tracker ID 0 was restored for device `99806266A844`.
`CONFIG_ESB_CLOCK_INIT=y` is required in this bench profile: the transmitter
previously stalled in TX without PHYEND when SDK clock/errata initialization
was omitted. With this setting, receiver USB HID contains actual tracker
quaternions, metadata and status packets. The temporary legacy-receiver
compatibility changes were removed and never flashed.

See the sibling receiver project's `config/UPDATE-VALIDATION.md` and
`build-artifacts/receiver-hid-*.json/jsonl` for receiver-side evidence.
SlimeVR Server/UI presentation and final mounting-axis calibration remain
unverified. The profile holds the radio clock for bench testing; battery
life and radio power-saving behavior have not been measured.

Subsequent Server diagnosis found that enabling SDK clock initialization alone
did not eliminate an intermittent TX stall: application per-packet clock/PLL
operations still competed with SDK ownership. Those application hardware
operations are now skipped when `CONFIG_ESB_CLOCK_INIT=y`. The unmodified
Server then created the tracker. See `SERVER-CONNECTION-FIX.md` for evidence
and the receiver capture from the final tracker-only correction. Final
mounting calibration and battery life remain outside this validation.
