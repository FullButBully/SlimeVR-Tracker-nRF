# Repeated System OFF investigation (2026-10-23)

## Current state: rolled back at the user's request

The user requested rollback of the preceding sleep fix and flashing.
The LSM6DSV completion polling and DSV-specific ODR/FIFO readback handler
were removed; its source is back to the original shared SW_RESET + 1 ms
shutdown implementation. The earlier automatic DIF return-to-OFF attempt
also remains removed. RTT console and periodic diagnostics stay enabled.

The exact saved pre-investigation binary
`build-artifacts/current-restored-final/tracker-p1-button-sleep.hex`
(SHA256 d1622765fa9cf97b04d812671a9750f27126c96a5f35b9602084f0f0524449e5)
was programmed using the currently attached external J-Link PLUS 602712071.
RTT shows tracker ID 1, LSM6DSV, 2 MHz SPI, +/-16 g and +/-4000 dps,
and 1147 samples per sensor with zero errors by the ten-second report.
Evidence: `build-artifacts/sleep-rollback-flash.log` and `sleep-rollback-rtt.log`.
The SDK-clock ownership, ESB shutdown, and inactive CS changes that already
existed in this saved binary are preserved. The remainder of this document
records investigation history and does not describe the current flashed image.

The user reported that the first shutdown worked, the second did not stay
asleep, and indicated working current varied between 1 and 5 mA (previously
approximately 2 mA). Current is measured externally at P6; no onboard
current measurement is available.

## Observed trace

`build-artifacts/second-sleep-failure-rtt.log` contains an OFF wake
(RESETREAS=0x100, button pressed), then a second long press and shutdown:

- 00:01:13.484: User shutdown requested.
- 00:01:13.577: System off requested.
- 00:01:13.589: ESB disabled, CS parked inactive, Powering off nRF.
- 00:01:13.607: Boot reset reason 0x400, button=0.

Nordic MDK RESET_RESETREAS_DIF_Pos=10 identifies 0x400 as debug-interface
wake. The existing main() only honored user-off boot state when the button
or reset pin was active, so this debug wake with button released fell
through into normal tracker operation and cleared the shutdown flag.
The capture connection itself can cause DIF; this trace does not prove
that all earlier current fluctuations without a debugger had this cause.

## Correction

On nRF devices defining RESET_RESETREAS_DIF_Msk, if user shutdown is enabled,
the retained shutdown flag is set (reboot_counter=0), the reset cause includes
DIF, and no button is pressed, main() now requests silent System OFF and
returns rather than treating the wake as a user power-on. Cold boot, software
reset, and an actual button wake retain their existing behavior.

Build and flash: build-tracker-hardware-p1-button-sleep-current. RTT console,
its original polling interval, periodic diagnostics, and blocking RTT mode
remain as requested by the user. ESB shutdown and inactive CS parking are
retained. SPI is 2 MHz P1, ranges +/-16 g and +/-4000 dps.

`build-artifacts/second-sleep-fix-boot-rtt.log` confirms the flashed image
boots, discovers LSM6DSV, initializes fusion, and records 570 samples per
sensor with zero errors at the first five-second health report.

Two-cycle physical testing is pending. The user was asked to power-cycle
after all SWD sessions ended, shut down and wake twice, and supply both
standby readings plus active current. Only receiver HID is captured during
that test, avoiding tracker SWD accesses. This is necessary because software
cannot make a debugger-requested powered domain consume true System OFF
current while that debugger continues holding it active.

Reference:
https://docs.nordicsemi.com/r/bundle/ps_nrf54l15/page/debug.html-debuginterfacemode
