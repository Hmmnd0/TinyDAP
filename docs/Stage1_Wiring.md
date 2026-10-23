# Stage 1 Breadboard Wiring

ESP32-S3-DevKitC-1-N8R8 + PCM5102A + 7-pin SPI SSD1306 OLED + Adafruit 4682
microSD (4-bit SDIO) + 5 buttons.

Pin numbers are **GPIO numbers**, as printed next to each header pin on the
DevKitC-1. Header order is from Espressif's
[DevKitC-1 v1.1 user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html).

## Pins to avoid

| GPIO | Why |
|---|---|
| 35, 36, 37 | Used by the octal PSRAM on N8R8; touching them crashes the board |
| 0, 3, 45, 46 | Strapping pins, read at boot |
| 19, 20 | Native USB (the port we flash and log through) |
| 43, 44 | UART0, wired to the board's UART port |
| 38 | Onboard RGB LED |

## Pin plan

### PCM5102A DAC (I2S)

| PCM5102A pin | Connect to | Notes |
|---|---|---|
| VIN / VCC | **3.3V** (+ rail) | On the N8R8 clone, 5Vin is dead when powered from the native USB port; the PCM5102A runs fine on 3.3 V |
| GND | GND | |
| BCK | GPIO **5** | Bit clock |
| LCK | GPIO **6** | Word (left/right) clock |
| DIN | GPIO **7** | Audio data |
| SCK | **GND** | No master clock: the PCM5102A makes its own. Skip if the SCK solder bridge on the front is already closed |
| FLT, DMP, FMT | GND | Skip any that are already set by the solder jumpers on the back |
| XMT | **3.3V** | Unmute. Skip if already set by its jumper |

**Before wiring the PCM5102A, check the solder jumpers on the back (H1L–H4L).**
If a jumper is soldered, that pin is already set; leave it unconnected,
because wiring it the opposite way shorts 3.3 V to ground. Leave the
board's `3.3V` pin unconnected; it's an output.

### OLED, SPI

| OLED pin | Connect to |
|---|---|
| GND | GND |
| VCC | **3.3V** |
| D0 (SCLK) | GPIO **12** |
| D1 (MOSI) | GPIO **11** |
| RES | GPIO **13** |
| DC | GPIO **9** |
| CS | GPIO **10** |

### microSD, Adafruit 4682 (4-bit SDIO)

| Breakout pin | Connect to |
|---|---|
| 3V | **3.3V** (never 5 V) |
| GND | GND |
| CLK | GPIO **39** |
| CMD | GPIO **40** |
| D0 | GPIO **41** |
| D1 | GPIO **8** (was 42; moved during the 4-bit investigation) |
| DAT2 | GPIO **14** (was 2) |
| D3 | GPIO **1** |
| DET | GPIO **21** (optional card detect) |

The breakout has pull-ups on all SD lines, so no resistors are needed. Keep
these wires short (under ~10 cm); SDIO runs at 20–40 MHz.

### Buttons

Each button goes between its GPIO and **GND**. The firmware enables the
ESP32's internal pull-ups, so no resistors are needed.

| Button | GPIO |
|---|---|
| Play / pause | **4** |
| Previous | **15** |
| Next | **16** |
| Volume up | **17** |
| Volume down | **18** |

## Layout

The groups line up with the header so the wires stay short:

```
          J1 (left header)                 J3 (right header)
          3V3  ── 3.3V rail                GND ── GND rail
          3V3                              TX  (avoid)
          RST                              RX  (avoid)
 Play     4                                1   SD D3
 DAC BCK  5                                2   SD DAT2
 DAC LCK  6                                42  SD D1
 DAC DIN  7                                41  SD D0
 Prev     15                               40  SD CMD
 Next     16                               39  SD CLK
 Vol+     17                               38  (RGB LED)
 Vol-     18                               37  (PSRAM, avoid)
          8                                36  (PSRAM, avoid)
          3   (avoid)                      35  (PSRAM, avoid)
          46  (avoid)                      0   (avoid)
 OLED DC  9                                45  (avoid)
 OLED CS  10                               48
 OLED D1  11                               47
 OLED D0  12                               21  SD DET
 OLED RES 13                               20  (USB)
          14                               19  (USB)
          5V  (unused)                     GND
          GND ── GND rail                  GND
```

1. **Breadboards:** the DevKitC is too wide for one breadboard to leave
   room on both sides. Put two breadboards side by side and straddle the
   board across the join, so each header row has free holes next to it.
2. **Power rails:** run **3V3** to one rail and **GND** to the other. The
   OLED, SD breakout, and DAC all take power from there.
3. **Placement:** put the DAC and OLED on the J1 side, the SD breakout on
   the J3 side next to GPIO 39–42, and the buttons along the free edge of
   the J1 board.
4. **Before powering:** check that nothing connects to 5 V, and that the
   DAC's VIN, the SD breakout's 3V and the OLED's VDD go to 3.3 V.

## Bring-up order

Connect and test one part at a time:

1. DevKit alone: flash over the **USB** port (not UART); logs appear.
2. PCM5102A: test tone in stereo.
3. Buttons.
4. OLED: browser UI.
5. microSD: 4-bit SDIO, then full playback.

The firmware needs a DevKitC board target with these pins before step 1.

## Bring-up firmware

`firmware/stage1/` is a small ESP-IDF project that checks the wiring before
the full player is ported. It reuses `components/player` (tone generator)
and `components/ui` (framebuffer and font) and the Cardputer's `i2s_sink`.

- OLED: status screen; five boxes along the bottom fill while each button is held
- PCM5102A: plays the first audio file in `firmware/stage1/flash_data/` on
  repeat through the real decoder (built into a read-only FAT partition, so
  music works before the SD card is wired), or a test tone if that folder
  is empty. The folder is git-ignored; put your own 44.1 kHz clip there
  (up to ~6 MB).
- Play: pause/resume · Next: music / tone L+R / tone left / tone right ·
  Prev: restart track (tone: 440 / 1000 / 220 Hz) · Vol+/-: 3 dB steps
- A button that reads as pressed at boot is logged as miswired

```bash
cd firmware/stage1 && idf.py build flash monitor
```

**First flash on a new DevKit:** the factory demo firmware ignores the
automatic reset, so put the board in download mode by hand: unplug USB,
hold BOOT, plug in, release BOOT. After flashing, press RESET once (the
board stays in download mode otherwise). Later flashes work without buttons.

Verified 2026-10-21: OLED, all five buttons (40 ms debounce; 15 ms let
presses register twice), and FLAC playback from flash through the PCM5102A.

**N8R8 board (YEJMKJ, dual USB-C), 2026-10-21:** same pin order as the
DevKitC-1. Use the USB-C port that enumerates as "USB JTAG_serial debug
unit" (Espressif, PID 0x1001); the other port is a WCH CH343 USB-UART that
the firmware's console doesn't use. esptool 5.4's fast reflash (it only
rewrites changed sectors) once wrote the app at the wrong offset after a
verify failure; if a flash log shows "Verification failed after fast
reflash", erase the app partition (`esptool erase-region 0x10000 0x200000`)
and flash again, then check with `esptool verify-flash`.

Verified 2026-10-21 on the N8R8 board: FLAC playback heard through the
PCM5102A with VIN on 3.3 V (with VIN on 5Vin the DAC had no power).

**microSD, 2026-10-21:** the 16 GB card (CID name "SDABC") mounts in 1-bit
SDMMC at 20 MHz and plays with 0 underruns at ~2,080 KB/s read (vs 1,450
KB/s over SPI on the Cardputer). 4-bit fails with a CRC error on the first
4-bit data read (SSR) at every clock from 40 MHz down to 400 kHz, so it is
not signal timing. Ruled out on the bench: every line connected to the
breakout's pull-ups, no line-to-line shorts, and no permutation of the
D1/DAT2/D3 GPIOs works. Next: try a different (name-brand) microSD card.

**2026-10-23:** a new SanDisk 32 GB card (CID "SE32G") behaves the same:
1-bit at 20 MHz works; 4-bit fails with a CRC error on the first 4-bit read
(SSR), also at 400 kHz. So it is not the card. Resoldering the breakout's
header didn't change it, and no assignment of GPIO 42/2/1/21 to D1-D3
works. ESP-IDF v6.1's SDMMC driver configures D1-D3 correctly for 4-bit
on a GPIO-matrix slot (checked in the source). Remaining suspects: one of
DAT1-DAT3 not reaching the card inside the breakout (socket contact), or a
GPIO path. Moving D1/DAT2/D3 to GPIO 8/14/47 fails the same way, and so
does SDMMC slot 0 instead of slot 1, so the ESP32 pins and slot are ruled
out too. What's left is the breakout itself (DAT1-DAT3 not reaching the
card's contacts in the socket) or the SSR read specifically. Next: a
continuity check from the breakout's D1/DAT2/D3 pins to the card socket.
Until then the firmware falls back to 1-bit, which is fast enough for
every format TinyDAP plays (~2 MB/s).

**2026-10-23, later:** a minimal standalone 4-bit mount test (no player
code) fails the same way on both ESP-IDF v6.1 and v5.5.6, so it is not our
firmware and not a v6.1 regression. A test that sampled DAT0-DAT3 as GPIO
during a 4-bit read saw only ~55 edges per line (the reads aborted early),
so it does not prove the card drives DAT1-DAT3; treat it as inconclusive.
Remaining suspects are hardware: DAT1-DAT3 between the breakout pins and the
card contacts, or pull-up strength (the breakout has 47k; Espressif
recommends 10k on CMD and D0-D3). Next tests: 10k pull-ups from D1/DAT2/D3
to 3.3 V, then a continuity check or a second breakout.

**2026-10-23, logic capture:** with the card switched to 4-bit and the host
left in 1-bit at 400 kHz, the firmware sampled CLK and DAT0-DAT3 as GPIO
and decoded the card's 4-bit stream for sector 0. The partition entry
(`00 82 03 00 0C ...`) and the `55 AA` signature arrive correctly on all
four lines (the decoder's framing was one byte off, the data itself is
right). So the card, breakout, wiring and ESP32 input pads all carry valid
4-bit data. 10k pull-ups were added on D1/DAT2/D3 (confirmed from firmware:
those lines now read high against the internal pull-down); 4-bit still
fails. The fault is in how the SDMMC host receives 4-bit data in this setup,
not the signals. Still open; 1-bit (~2 MB/s) is the working fallback.

**PSRAM, 2026-10-23:** octal PSRAM enabled (`CONFIG_SPIRAM_MODE_OCT`, 80 MHz).
Boot log: AP 64 Mbit generation-3 device, 8 MB found, memory test OK; 8 MB
added to the heap with 32 KB of internal RAM reserved for DMA. The boot
memory test adds ~0.4 s to startup.

**2026-10-23, host-contention test:** the same capture run twice on sector 0
at 400 kHz, once with the host in 1-bit and once with the host genuinely in
4-bit (`set_bus_width(4)`). Both captures are identical (same nibbles, same
55AA position, same handful of decode slips from the software sampler), so
the SDMMC host is not driving or disturbing DAT1-3 in 4-bit mode. Hypothesis
ruled out; root cause still open.
