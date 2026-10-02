# TinyDAP firmware

ESP-IDF project for the ESP32-S3. Stage 0 target: M5Stack Cardputer-Adv.

## Layout

```
firmware/
├── main/                     ESP32 entry point, FreeRTOS tasks, board pin map
│   ├── main.c
│   └── board_cardputer_adv.h
├── components/
│   └── player/               portable player core — no ESP-IDF dependencies
│       ├── include/tinydap/
│       │   ├── pcm_ring.h    lock-free SPSC PCM ring buffer
│       │   ├── audio_sink.h  audio output abstraction (ES8311/PCM5102A/CS43131/WAV)
│       │   └── tone.h        test-tone generator (decoder stand-in)
│       ├── pcm_ring.c
│       └── tone.c
└── host/                     native macOS/Linux build of components/player
    ├── test_pcm_ring.c       unit + multithreaded stress tests
    ├── tone_to_wav.c         pipeline demo: tone -> ring -> WAV file
    └── wav_sink.c            audio_sink that writes WAV
```

This follows the platform split in write-up §20: everything in
`components/player` must stay free of ESP-IDF/FreeRTOS includes so it builds
on the host and ports unchanged to the Stage 1 breadboard and Rev A.

## Current state

The task layout from write-up §18 is in place. A 440 Hz test tone plays
decoder → PCM ring → audio output → I2S → ES8311 → 3.5 mm jack (mono),
verified on hardware. Every 2 s the `stats` task logs ring fill, underrun
count, free internal heap, and per-task stack headroom.

Next: the storage task reads a WAV from microSD in place of the tone.

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
