# Stage 0 Findings — Cardputer-Adv

Running log of what has been measured, verified, and learned on the
Stage 0 platform. The project specification remains
[TinyDAP_Project_Writeup.md](TinyDAP_Project_Writeup.md); this file records
evidence. Newest entries are at the top of each section.

Hardware: M5Stack Cardputer-Adv (K132-ADV), ESP32-S3 rev v0.2, 8 MB
embedded flash, **no PSRAM**. Firmware: ESP-IDF v6.1.

---

## Milestones

| Date | Milestone | Commit |
|---|---|---|
| 2026-10-02 | FLAC 24/96 stress test passes: 0 underruns, ES8311 runs at 96 kHz | — |
| 2026-10-02 | FLAC playback (dr_flac), tags on Now Playing | `574ae93` |
| 2026-10-02 | SD card browser UI + WAV playback | `82b3caf` |
| 2026-10-02 | 440 Hz test tone through I2S → ES8311 → 3.5 mm jack | `4c3a815` |
| 2026-10-02 | First flash: FreeRTOS task layout, PCM ring, stats logging (0 underruns, null sink) | `2d6edb0` |

---

## Measurements

All with the decoder and audio-output tasks on core 1, UI/input/stats on
core 0 (write-up §18), 32 KB PCM ring in internal SRAM.

### Playback

| Test | Underruns | PCM ring fill | Notes |
|---|---|---|---|
| FLAC 16/44.1 stereo, rapid skipping through 10 tracks in ~9 s | 0 | 81–100% | Track start feels instant |
| FLAC 16/44.1 stereo, continuous | 0 | 78–100% | |
| WAV 16/44.1 stereo, 4+ min continuous | 0 | 84–100% | |
| WAV 16/48 stereo (Chip Rack) | *not yet logged* | | I2S retune 44.1 ↔ 48 kHz implemented; confirm by ear and log |
| **FLAC 24/96 stereo, rapid switching: 20 track changes in ~13 s** | **0** | **65–93%** | Upsampled test files, ~2.8 Mbit/s |
| FLAC 24/96 stereo, continuous | 0 | 65–93% | Plays at correct speed; audibly fine on headphones |

Track start latency is dominated by opening the file; the audio task waits
for the ring to be half full, which FLAC fills in a few milliseconds.

#### 24/96 FLAC: what the numbers tell us

24/96 is the write-up's worst case (§4: ~576 KB/s of PCM), roughly 3x the
decode work and SD bandwidth of 16/44.1.

- **The ESP32-S3 keeps up with the worst case.** Zero underruns, even while
  hammering track changes, with the decoder at priority 22 on core 1. Rev A
  uses the same CPU, with faster 4-bit SDMMC, so this carries over.
- **The ring runs lower, and that is expected, not a problem yet.** The
  ring is a fixed 32 KB, and its *time* coverage shrinks with sample rate:

  | Source | Ring bytes/s (16-bit stereo out) | 32 KB ring holds |
  |---|---|---|
  | 44.1 kHz | 176 KB/s | ~186 ms |
  | 96 kHz | 384 KB/s | **~85 ms** |

  At 96 kHz the audio task drains the ring 2.2x faster, so the same bytes
  give less than half the protection against a slow SD read. Fill dipped to
  65% (~55 ms of audio in hand) but never emptied.
- **Recommendation:** raise the PCM ring to 64 KB (~170 ms at 96 kHz).
  There is ~130 KB of internal heap free while playing 24/96, and start
  latency is unaffected because playback begins at half full. Then run the
  worst-case SD latency test (fragmented card) at 96 kHz.
- **Decoder stack does not grow with bit depth.** ~7.2 KB used for both
  16/44.1 and 24/96; the 16 KB stack has ~9 KB headroom.
- **dr_flac heap grows only slightly** with 4608-frame blocks: ~49 KB vs
  ~45 KB, flat across all track changes.

### Memory

| State | Internal heap free | Min ever |
|---|---|---|
| Idle, browser UI (ST7789 frame buffer allocated) | 187 KB | — |
| WAV playing | 182 KB | 175 KB |
| FLAC 16/44.1 playing, 8 KB decoder stack | 142 KB | 141 KB |
| FLAC 16/44.1 playing, 16 KB decoder stack | 134 KB | 130 KB |
| Idle after reboot, 16 KB decoder stack | 179 KB | 170 KB |
| FLAC 24/96 playing (4608-frame blocks) | 130 KB | 130 KB |

- dr_flac uses **~45 KB of heap** per open 16/44.1 track and **~49 KB** for
  24/96 with 4608-frame blocks (decoded-block buffer sized by max block
  size × channels × 4 bytes, plus read buffer).
- Free heap stays flat across dozens of track changes: no leak.
- The ST7789 driver holds a **61 KB** DMA frame buffer (240×128×2). On the
  final SSD1306 the frame is 1 KB, so Rev A gains ~60 KB of internal SRAM.

### Task stacks (free bytes, high-water mark)

| Task | Stack | WAV | FLAC 16/44.1 | FLAC 24/96 |
|---|---|---|---|---|
| decoder | 8 KB | 5.5 KB | **0.96 KB** | — |
| decoder | 16 KB | — | 9.1 KB | 9.1 KB |
| audio_out | 4 KB | 3.3 KB | 3.3 KB | 3.3 KB |
| ui | 8 KB | 4.5 KB | 4.5 KB | 4.5 KB |
| input | 3 KB | 1.7 KB | 1.7 KB | 1.7 KB |

**Finding:** dr_flac decodes on the stack, ~7.2 KB for both 16/44.1 and
24/96. The 8 KB decoder stack was nearly exhausted and was raised to 16 KB.

### Decoder correctness

- All 14 tracks of a 16/44.1 FLAC album decoded by TinyDAP's decoder
  (`host/build/decode_to_wav`) are **bit-identical** to macOS `afconvert`
  output (~58 min of audio). Vorbis comment tags (title/artist/album) read
  correctly.
- 24/96 FLAC test files decode with the full frame count on the host.

---

## Verified hardware facts (Cardputer-Adv)

| Item | Value | Source / how verified |
|---|---|---|
| ES8311 I2C | SDA 8, SCL 9, address 0x18 | M5Unified pin table; chip ID reads `8311` |
| ES8311 I2S | port 1, BCLK 41, WS 43, DOUT 42, DIN 46, no MCLK | M5Unified; audio plays |
| ES8311 clocking | MCLK derived from BCLK (32×fs, pre-multiply ×8) | Register sequence from M5Unified |
| ES8311 at 96 kHz | Works: internal MCLK 24.576 MHz from a 3.072 MHz BCLK | 24/96 FLAC plays at correct speed |
| Headphone output | 3.5 mm jack, **mono** (ES8311 is single-channel) | M5Stack docs, ES8311 |
| TCA8418 keyboard | I2C 0x34 on the same bus, 7×8 matrix, INT GPIO 11 | M5Cardputer library; keys work |
| ST7789 LCD | SPI3: MOSI 35, SCLK 36, DC 34, CS 37, RST 33, BL 38; 240×135, inverted, gap (40, 53), swap XY + mirror X | M5GFX config; UI readable and navigable on device |
| microSD | SPI2: SCK 40, MISO 39, MOSI 14, CS 12 | EMBER; mounts a 16 GB FAT32 card |
| External I2S (for PCM5102A) | port 0, BCLK 5, WS 6, DOUT 3 | EMBER (not yet tested here) |
| Flashing | Native USB-Serial/JTAG; `idf.py flash` auto-resets, no button needed | Every flash so far |

---

## Findings and gotchas

- **ES8311 output is quiet.** M5Unified applies 16× (+24 dB) software gain
  on this board. Codec digital volume 0 dB with a -20 dBFS tone is
  comfortable; music defaults to -12 dB. Headroom up to +32 dB exists in the
  codec if needed.
- **ES8311 is mono.** Stereo is folded to (L+R)/2 in the decoder task rather
  than letting the codec drop the right channel. Stereo testing in Stage 0
  needs the PCM5102A on the external I2S port.
- **Stage 0 can't judge sound quality.** The mono ES8311 path sounds better
  on headphones than the built-in speaker, but stereo imaging, 24-bit
  output (currently truncated to 16-bit for the ES8311), and noise floor
  can only be evaluated on the PCM5102A (Stage 0/1) and CS43131 (Stage 2).
  Stage 0 validates timing, memory, and correctness, not fidelity.
- **exFAT is disabled** in ESP-IDF's FatFs (`FF_FS_EXFAT 0`). Cards must be
  FAT32; 64 GB+ cards usually ship exFAT and need reformatting.
- **macOS writes `._*` AppleDouble files** to FAT cards even with
  `COPYFILE_DISABLE=1`. They must be deleted after copying or the browser
  lists them as broken tracks. (Firmware already hides dot-files.)
- **`idf.py monitor` holds the serial port.** A second flash fails with
  "port is busy" until the monitor is closed (Ctrl+]) or flashed from inside
  it (Ctrl+T, Ctrl+F).
- **ESP-IDF builds with `-Werror` on format truncation**; size `snprintf`
  buffers generously.

---

## Carry-forward for Stage 1 / Rev A

- **Same CPU and SRAM** as ESP32-S3-PICO-1-N8R8: timing and internal-memory
  numbers here are a worst case for Rev A (Rev A also gets 8 MB PSRAM, 4-bit
  SDMMC, and a 1 KB OLED frame instead of 61 KB).
- **Octal PSRAM (N8R8) occupies GPIO 33–37.** The Cardputer's LCD uses
  exactly those pins; Stage 1 and Rev A pin maps must avoid them. Confirm
  against the PICO-1 datasheet when drawing the Stage 1 pin plan.
- **CS43131 will likely need a real MCLK**; `i2s_sink` already supports an
  MCLK pin.
- Keep the decoder stack at ≥16 KB until 24-bit / large-block FLAC is
  measured.

---

## Open questions / next tests

- [x] FLAC 24/96: 0 underruns under rapid switching; ring 65–93%; decoder
      stack 7.2 KB; dr_flac heap ~49 KB
- [x] ES8311 at 96 kHz (internal MCLK 24.576 MHz) — works
- [ ] Raise PCM ring to 64 KB and re-measure 24/96 fill levels
- [ ] Measure decoder CPU time per block (currently inferred only from
      ring fill)
- [ ] Real hi-res source material (e.g. 2L test bench) vs upsampled files
- [ ] PCM5102A on the external I2S port: stereo, 24-bit output
- [ ] MP3 via minimp3
- [ ] SD read latency worst case (fragmented card), Wi-Fi active during playback
