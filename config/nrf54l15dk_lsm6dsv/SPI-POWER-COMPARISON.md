# P1 / P2 SPI current comparison

Both builds use the same tracker.conf, 2 MHz SPI mode 3, LSM6DSV +/-16 g
and +/-4000 dps, nominal 120 Hz (measured approximately 116.88 Hz),
INT1=P1.11, and default +8 dBm ESB output. UART20 is disabled in the
shared wiring overlay so it does not occupy P1 SPI pins or contribute
different driver overhead between builds. No terminal may open either
DK VCOM port while P1.04..P1.07 are being used for SPI: the debugger
tri-states its UART lines only while no terminal is connected.

| Signal | P2 profile (SPIM00) | P1 profile (SPIM21) |
| --- | --- | --- |
| SCK | P2.06 | P1.04 |
| SDI / MOSI | P2.08 | P1.05 |
| SDO / MISO | P2.04 | P1.06 |
| CS | P2.10 | P1.07 |
| INT1 | P1.11 | P1.11 |

Build with `tools/build-dk.ps1 -App tracker -Wiring hardware` for P2 or
`-Wiring hardware-p1` for P1. Their separate build directories preserve
both alternatives. Pin assignments live in hardware.overlay and
hardware-p1.overlay. This compares the two supported controller/port
combinations, not a pin-only change on the same SPI peripheral.

Keep 1.8 V, receiver distance, IMU movement, LED state, and meter connection
identical. Include the IMU supply downstream of the meter if measuring
the whole tracker. End all J-Link/RTT/debug sessions and power-cycle before
taking each reading, so debugger power-domain retention does not bias it.
Wait for IMU startup recovery before recording steady-state current.
This existing bench profile holds the radio clock and has logging enabled;
it measures that profile, not minimum achievable tracker consumption.

The prior flashed P2 build had UART20 enabled in devicetree. Use the newly
matched P2 comparison image for a controlled A/B measurement rather than
assuming a previous current reading represents this matched configuration.

Official DK UART routing documentation:
https://docs.nordicsemi.com/r/bundle/ug_nrf54l15_dk/page/ug/nrf54l15_dk/program_debug/virtual_serial_ports.html

The user completed P1 rewiring and the P1 image was flashed on 2026-10-22.
`build-artifacts/p1-validation-rtt.log` confirms LSM6DSV detection,
2 MHz SPI, +/-16 g and +/-4000 dps, 116.88 Hz, and 1733 samples per sensor
with zero packet errors by the 15-second health report. Receiver evidence
in `../SlimeVR-Tracker-nRF-Receiver/build-artifacts/p1-spi-validation-hid.json`
contains 87 quaternion packets plus 147 metadata packets over 15.094 seconds.
This was a mostly stationary capture, not a new full motion/cold-boot test.
Both bounded capture tools exited and no Python/J-Link processes remained
at the final process check. Power-cycle after debugging before measuring.

Matched images are `build-artifacts/spi-power-comparison/tracker-p1.hex`
and `tracker-p2.hex`, with their ELF/map and saved Kconfig configurations.
The saved Kconfig files compare equal; the devicetree selects different
SPI controllers and pins. `build-artifacts/final` now contains the flashed
P1 image. Current readings are to be supplied by the user's ammeter.
