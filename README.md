# TinyDAP

How small can we make a lossless music player?

TinyDAP is a pocket-sized, battery-powered digital audio player built around
the ESP32-S3. It plays FLAC, MP3, and WAV from microSD through a
high-fidelity Cirrus Logic CS43131 DAC/headphone driver, with a 0.96"
128×64 OLED and physical buttons, targeting a custom PCB of roughly
32 × 27 mm.

**Status:** Stage 0 — FLAC and WAV playback with an SD browser UI running on the Cardputer-Adv.

## Rev A hardware

Current intended Rev A parts, from §21 of the
[project write-up](docs/TinyDAP_Project_Writeup.md) (the source of truth;
not yet a frozen production BOM).

| Function | Part | Role |
|---|---|---|
| MCU | Espressif ESP32-S3-PICO-1-N8R8 | Main processor, Wi-Fi/BLE, native USB, I²S, SDMMC; 8 MB flash + 8 MB PSRAM in a 7 × 7 mm SiP (N8R2 as sourcing fallback) |
| DAC / headphone driver | Cirrus Logic CS43131-CWZR | Final DAC with integrated headphone driver, 42-WLCSP |
| DAC, prototype package | Cirrus Logic CS43131-CNZ | 5 × 5 mm QFN for the Stage 2 carrier PCB |
| Display | Winstar WEO012864D | 0.96" 128 × 64 white SSD1306 bare COG OLED, 4-wire SPI |
| microSD socket | GCT MEM2075-00-140-01-A | Storage, 4-bit SDMMC |
| USB | GCT USB4105-GF-A-060 | USB-C: charging, flashing, debug |
| Charger | TI BQ25185DLHR | 1S LiPo charging |
| Main regulator | TI TPS63031DSKR + inductor | 3.3 V buck-boost |
| Audio regulators | TBD low-noise 1.8 V rail(s) | CS43131 supplies |
| Level shifting | TBD | 3.3 V ESP32 ↔ 1.8 V CS43131 where required |
| Headphone jack | Same Sky SJ-43516-SMT-TR | 3.5 mm TRS stereo output |
| Buttons | GCT SWT0005-015516SSA ×5 | Side controls: play/pause, previous, next, volume ± (optional power/hold) |
| Battery | 400–600 mAh 1S LiPo pouch | Portable power, behind the PCB |
| ESD | TBD | USB, headphone, and user-accessible protection |
| Passives | 0201/0402/0603 | Decoupling, filtering, bias, power |

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
- **Stage 0 measurements and findings:** [docs/Stage0_Findings.md](docs/Stage0_Findings.md)
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
