# TinyDAP firmware

ESP-IDF project for the ESP32-S3. Stage 0 target: M5Stack Cardputer-Adv.

## Layout

```
firmware/
├── main/                     ESP32 / Cardputer-Adv platform code
│   ├── main.c                startup, input/UI/stats tasks, key mapping
│   ├── player.c              playback engine: decoder + audio tasks (§18)
│   ├── i2s_sink.c            I2S audio output
│   ├── es8311.c              ES8311 codec driver
│   ├── display.c             ST7789 driver, shows the 128x64 UI scaled up
│   ├── keyboard.c            TCA8418 keyboard driver
│   ├── sdcard.c              microSD mount (SPI, FAT32)
│   └── board_cardputer_adv.h pin map
├── components/
│   ├── player/               portable core — no ESP-IDF dependencies
│   │   ├── pcm_ring.c        lock-free SPSC PCM ring buffer
│   │   ├── decoder.c         WAV/FLAC/MP3 decoders behind one interface
│   │   ├── third_party/      dr_flac.h, minimp3.h (public domain)
│   │   ├── wav.c             WAV header parser
│   │   ├── browser.c         folder listing, natural sort
│   │   └── tone.c            test-tone generator
│   └── ui/                   portable UI — no ESP-IDF dependencies
│       ├── fb.c, font5x7.c   128x64 1-bit framebuffer (SSD1306 layout) + font
│       └── ui_app.c          SD browser and Now Playing screens
└── host/                     native macOS/Linux build of the portable code
    ├── test_pcm_ring.c       unit + multithreaded stress tests
    ├── tone_to_wav.c         pipeline demo: tone -> ring -> WAV file
    ├── ui_demo.c             drives the UI on a real folder, saves BMP screens
    ├── decode_to_wav.c       decodes a file with TinyDAP's decoders, for reference checks
    └── stack_check.c         measures decoder peak stack use against a budget
```

This follows the platform split in write-up §20: everything in
`components/` must stay free of ESP-IDF/FreeRTOS includes so it builds on
the host and ports unchanged to the Stage 1 breadboard and Rev A. The UI
draws the final 128x64 OLED layout; on the Cardputer it is scaled onto the
240x135 LCD.

## Current state

Browses the microSD card and plays FLAC (dr_flac) and MP3 (minimp3) with
title/artist/album tags, and WAV files (16/24-bit, mono/stereo, 8–96 kHz; I2S retunes per
track) through I2S → ES8311 → 3.5 mm jack, with gapless auto-advance
through the folder and optional folder repeat. FLAC output is verified bit-identical to macOS `afconvert` on the
host (`host/build/decode_to_wav`). Stereo is folded to mono for the ES8311.
Verified on hardware with 0 underruns up to 24-bit/96 kHz FLAC. Every 5 s
the `stats` task logs state, ring fill, underruns, decoder load (total and
SD-read share, and speed vs real time), and free internal heap; every 30 s,
per-task stack headroom. Results are tracked in
[docs/Stage0_Findings.md](../docs/Stage0_Findings.md).

Next: a mixed-format soak, button-stress testing, and the PCM5102A for stereo.

### Controls (Cardputer-Adv)

| Key | Action |
|---|---|
| `;` / `.` | Up / down (hold to repeat) |
| `/` or Enter | Open folder / play |
| `,`, `` ` `` or Backspace | Back |
| Space | Play / pause |
| `n` / `b` | Next / previous track |
| `=` / `-` | Volume up / down |
| `m` | Browser ↔ Now Playing |
| `r` | Folder repeat on/off |
| `o` | Screen off/on |

The screen runs at 50% backlight and turns off after 30 s without input
(`SCREEN_BRIGHTNESS_PCT` and `SCREEN_TIMEOUT_MS` in `main/main.c`). While it
is off, play/pause, next/previous, and volume still work without waking it;
any other key only wakes the screen.

### Preparing an SD card

FAT32 only (exFAT is disabled in ESP-IDF's FatFs). FLAC, MP3, and WAV play
directly; other formats can be converted with `../tools/to_wav.sh
<source> <dest>` (macOS `afconvert`, 16-bit 44.1 kHz WAV, keeps folder
structure). Copy to the card, then remove macOS
`._*` files from the copied folders.

## ESP32 build

One-time ESP-IDF setup on macOS (see Espressif's "Get Started" guide for
current details and pick the latest stable release):

```bash
brew install cmake ninja dfu-util ccache python3
```

```bash
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
```

```bash
cd ~/esp/esp-idf && git checkout <latest-stable-tag> && git submodule update --init --recursive && ./install.sh esp32s3
```

Each new terminal:

```bash
. ~/esp/esp-idf/export.sh
```

Build, flash, and watch the log (Cardputer-Adv connected over USB-C):

```bash
idf.py build flash monitor
```

## Host build (no hardware)

```bash
cmake -S host -B host/build && cmake --build host/build && ctest --test-dir host/build
```

```bash
./host/build/tone_to_wav tone.wav
```

```bash
./host/build/ui_demo ~/Music/some-folder /tmp/screens "dss"
```

## Testing without hardware

There is no emulator that runs this firmware end to end with real audio
timing. The options, in order of usefulness:

1. **Host build (`host/`)** — the main tool. Decoder, buffering, metadata,
   playlist, and player-state logic run natively on the Mac, with unit tests
   and sanitizers, and output goes to WAV files you can listen to.
2. **Espressif QEMU (`idf.py qemu monitor`)** — boots ESP32-S3 firmware and
   is useful for checking that tasks start, logging, and crash debugging.
   It does not emulate I2S, the ES8311, or SD, so no audio.
3. **Wokwi** — ESP32-S3 simulator with some displays and an SD card. Useful
   for UI experiments, but its peripheral support doesn't match TinyDAP's
   hardware (ES8311, CS43131) and it can't judge real-time audio behavior.

Underruns, SD latency, I2S clocking, and audio quality can only be
validated on real hardware.
