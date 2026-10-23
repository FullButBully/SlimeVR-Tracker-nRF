# Custom nRF54L15 board: unchanged P1 image first test

On 2026-10-22 the user's custom board was programmed through J-Link PLUS
serial 602712071, using `build-artifacts/spi-power-comparison/tracker-p1.hex`
without rebuilding or changing firmware. SHA256:
`40ba94af5535e3c903364ec0ae7737cbd619a83846071d4a220fd3b2994e0539`.
All four HEX segments were read back and matched byte for byte; see
`build-artifacts/custom-board-original-p1-verify.json`.

RTT (`build-artifacts/custom-board-original-p1-rtt.log`) confirmed:

- New device address 8C66EB168323.
- LSM6DSV WHO_AM_I 0x70 on the existing P1 wiring.
- SPI 2 MHz; +/-16 g, +/-4000 dps; both sensors 116.88 Hz.
- VQF initialized; 2306 samples per sensor with zero packet errors at
  the final 20-second health report.
- Internal VDD measurement approximately 3312 mV; probe VTref 3304 mV.
  This differs from the earlier 1.8 V DK test, so compare current readings
  only after accounting for actual supply voltage and board configuration.
- Pairing was still in progress at the end of the capture. Repeated
  `Pairing request received` messages do not mean `Paired`; Server
  connectivity and radio stability of this board are not yet validated.

The unchanged DK-targeted image runs on this board for this bounded test;
this is not full board-port qualification or cold-start validation.
J-Link emitted an old probe-firmware warning about I/D-cache debug handling.
Programming/readback and observed runtime succeeded, but no probe firmware
update was performed. All test sessions were closed afterward.
