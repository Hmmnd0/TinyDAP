# TinyDAP

TinyDAP is an ESP32-S3-based portable lossless digital audio player.

Before making architectural, firmware, hardware, or component decisions,
read:

docs/TinyDAP_Project_Writeup.md

Treat that document as the current project specification and design
history.

Primary target:
- ESP32-S3
- ESP-IDF
- FreeRTOS
- microSD local storage
- FLAC/MP3/WAV playback
- I2S audio
- CS43131 final DAC/headphone driver
- SSD1306 128x64 OLED over SPI
- USB-C
- LiPo power
- physical controls

Development platforms:
- M5Stack StickS3 (K150) for Stage 0 ESP32-S3 firmware/audio development
- ESP32-S3 breadboard for hardware prototype
- Raspberry Pi optionally as a known-good I2S/reference test platform
- Custom CS43131 QFN carrier before final PCB

Do not silently change component choices or architecture documented in
the project specification. If a change appears beneficial, explain the
reason and tradeoffs first.