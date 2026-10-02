# TinyDAP

How small can we make a lossless music player?

TinyDAP is a pocket-sized, battery-powered digital audio player built around
the ESP32-S3. It plays FLAC, MP3, and WAV from microSD through a
high-fidelity Cirrus Logic CS43131 DAC/headphone driver, with a 0.96"
128×64 OLED and physical buttons, targeting a custom PCB of roughly
32 × 27 mm.

**Status:** Stage 0, firmware bring-up on development hardware.

## Target hardware

| Function | Part |
|---|---|
| MCU | ESP32-S3-PICO-1-N8R8 (8 MB flash, 8 MB PSRAM) |
| DAC / headphone driver | Cirrus Logic CS43131 |
| Display | 0.96" 128×64 SSD1306 OLED over SPI |
| Storage | microSD (SDMMC) |
| Power | 1S LiPo, TI BQ25185 charger, TI TPS63031 buck-boost |
| I/O | USB-C, 3.5 mm headphone jack, 5 buttons |

## Development stages

| Stage | Hardware | Goal |
|---|---|---|
| 0 | M5Stack Cardputer-Adv (+ PCM5102A for stereo) | Firmware architecture and audio pipeline |
| 1 | ESP32-S3-DevKitC-1 N8R8 breadboard | microSD + SPI OLED + buttons + PCM5102A |
| 2 | Custom CS43131-CNZ carrier PCB | Validate the final DAC |
| 3 | Battery / charger / regulator test | Portable power and noise |
| 4 | TinyDAP Rev A PCB | Everything on one board |

## Repository

```
docs/       Project write-up: design decisions, BOM, and roadmap
firmware/   ESP-IDF firmware (ESP32-S3) and host-side tests
hardware/   Schematics and PCB (not started)
```

- **Design and specification:** [docs/TinyDAP_Project_Writeup.md](docs/TinyDAP_Project_Writeup.md)
- **Building and flashing the firmware:** [firmware/README.md](firmware/README.md)

## Firmware at a glance

Built on ESP-IDF and FreeRTOS. Playback is a pipeline of prioritized tasks
(storage → decoder → lock-free PCM ring buffer → I2S/DMA → DAC) so UI,
filesystem, and network work can never starve audio output. The player core
is kept free of ESP-IDF dependencies so it also builds and is tested on a
desktop.

## Acknowledgements

[EMBER](https://github.com/HorseyofCoursey/EMBER) by HorseyofCoursey, an
MIT-licensed music player for the Cardputer-Adv, is used as a Stage 0
reference implementation.
