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
| 2026-10-02 | 24/96 FLAC fixed (read-ahead + 64 KB ring), decoder load instrumented | `e241123` |
| 2026-10-02 | 24/96 FLAC first test: short run clean, then 503 underruns on continued play | `29548a9` |
| 2026-10-02 | FLAC playback (dr_flac), tags on Now Playing | `574ae93` |
| 2026-10-02 | SD card browser UI + WAV playback | `82b3caf` |
| 2026-10-02 | 440 Hz test tone through I2S → ES8311 → 3.5 mm jack | `4c3a815` |
| 2026-10-02 | First flash: FreeRTOS task layout, PCM ring, stats logging (0 underruns, null sink) | `2d6edb0` |

---

## Measurements

All with the decoder and audio-output tasks on core 1, UI/input/stats on
core 0 (write-up §18), PCM ring in internal SRAM (32 KB until the 24/96 fix,
64 KB after).

"Decode" is the share of wall time the decoder task spends inside
`decoder_read` (storage reads + FLAC decoding); "SD" is the storage-read part.
"x realtime" is audio produced per second of decoder busy time.

### Playback

| Test | Underruns | PCM ring fill | Notes |
|---|---|---|---|
| FLAC 16/44.1 stereo, rapid skipping through 10 tracks in ~9 s | 0 | 81–100% | Track start feels instant |
| FLAC 16/44.1 stereo, continuous | 0 | 78–100% | |
| WAV 16/44.1 stereo, 4+ min continuous | 0 | 84–100% | |
| WAV 16/48 stereo (Chip Rack) | *not yet logged* | | I2S retune 44.1 ↔ 48 kHz implemented; confirm by ear and log |
| FLAC 24/96, 20 track changes in ~13 s (32 KB ring, 4 KB stdio reads) | 0 | 65–93% | Looked fine in a short run… |
| FLAC 24/96, continuous (32 KB ring, 4 KB stdio reads) | **503** | 25–96% | …audible lag on longer play; sawtooth fill pattern |
| **FLAC 24/96, continuous (64 KB ring, 16 KB read-ahead)** | **0** | **84–98%** | Fixed; plays at correct speed |

#### Decoder load: same track at two resolutions

After the fix (64 KB ring, 16 KB read-ahead), "One More Time", ~30 s each:

| | 16/44.1 FLAC | 24/96 FLAC | Ratio |
|---|---|---|---|
| Underruns | 0 | 0 | |
| PCM ring fill | 89–100% | 84–98% | |
| **Decoder busy (decode)** | **17–20%** | **42–46%** | ~2.4x |
| — storage reads (SD) | 8–10% | 24–28% | ~2.8x (= bitrate ratio) |
| — FLAC decoding (CPU) | ~9% | ~18% | ~2x (≈ sample-rate ratio) |
| **x realtime** | **4.9–5.8x** | **2.1–2.3x** | |
| Decoder stack free (of 16 KB) | 9.2 KB | 9.2 KB | |

Track start latency is dominated by opening the file; the audio task waits
for the ring to be half full, which FLAC fills in a few milliseconds.

#### What the numbers tell us

24/96 is the write-up's worst case (§4: ~576 KB/s of PCM).

- **The first 24/96 "pass" was premature.** A short run with track
  skipping showed 0 underruns, but the fill already had a sawtooth pattern
  (96 → 84 → 75 → 65% and back). On continued playback the decoder fell
  behind and accumulated 503 underruns: audible lag. Lesson: judge
  throughput on multi-minute continuous playback, not short runs, and treat
  a steadily sinking fill level as a warning even at 0 underruns.
- **Storage, not CPU, was the bottleneck.** With timing instrumentation,
  SD reads are more than half of the decoder's busy time at 24/96. The
  actual FLAC math is cheap: ~9% of one core at 16/44.1, ~18% at 24/96.
  SD-over-SPI delivers roughly 1.4 MB/s effective here.
- **Fix:** read the file through a 16 KB heap read-ahead buffer (one large
  multi-sector SD transfer per refill instead of 4 KB stdio reads), and
  double the PCM ring to 64 KB (~170 ms at 96 kHz instead of ~85 ms).
- **Headroom after the fix:** the worst case runs 2.1–2.3x faster than
  playback (decoder idle ~55% of the time); CD quality runs ~5x.
- **Ring time coverage shrinks with sample rate:**

  | Source | Ring bytes/s (16-bit stereo out) | 32 KB ring | 64 KB ring |
  |---|---|---|---|
  | 44.1 kHz | 176 KB/s | ~186 ms | ~370 ms |
  | 96 kHz | 384 KB/s | ~85 ms | ~170 ms |

- **For Rev A:** CPU figures carry over (same chip). The SD share is an
  SPI-mode number; the write-up's 4-bit SDMMC (§9) should cut it sharply,
  and the read-ahead buffer can move to PSRAM.

### Memory

| State | Internal heap free | Min ever |
|---|---|---|
| Idle, browser UI (ST7789 frame buffer allocated) | 187 KB | — |
| WAV playing | 182 KB | 175 KB |
| FLAC 16/44.1 playing, 8 KB decoder stack | 142 KB | 141 KB |
| FLAC 16/44.1 playing, 16 KB decoder stack | 134 KB | 130 KB |
| Idle after reboot, 16 KB decoder stack | 179 KB | 170 KB |
| FLAC 24/96 playing (4608-frame blocks), 32 KB ring | 130 KB | 130 KB |
| Idle, 64 KB ring | 146 KB | 137 KB |
| FLAC 16/44.1 playing, 64 KB ring + 16 KB read-ahead | ~90 KB | 77 KB |
| FLAC 24/96 playing, 64 KB ring + 16 KB read-ahead | 78–86 KB | 78 KB |

- dr_flac uses **~45 KB of heap** per open 16/44.1 track and **~49 KB** for
  24/96 with 4608-frame blocks (decoded-block buffer sized by max block
  size × channels × 4 bytes, plus read buffer).
- Free heap stays flat across dozens of track changes: no leak.
- **Internal RAM is now the tightest resource on the Cardputer** (no
  PSRAM): ~78 KB free at the 24/96 worst case. Rev A's PSRAM can hold the
  read-ahead buffer and other non-latency-critical data (§2).
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

**Incident: stack overflow from a build setting.** The first attempt at
larger SD reads raised dr_flac's `DR_FLAC_BUFFER_SIZE` to 16 KB. dr_flac
builds that cache inside a struct *on the stack* while opening a file, so
every FLAC open overflowed the 16 KB decoder stack; FreeRTOS's overflow
check panicked and rebooted the device. Host tests passed because macOS
threads have 8 MB stacks. Fixed by keeping dr_flac's default 4 KB cache and
doing read-ahead in our own heap buffer.

**Guard:** `host/stack_check` runs a decode on a pattern-filled thread stack
and reports peak use against a 12 KB budget (FreeRTOS high-water-mark
style). Current decoder: ~5.9 KB on the host (~7.2 KB measured on device).
The bad 16 KB setting reports 18.2 KB and fails. Run it under ctest with
`cmake -DTINYDAP_TEST_FLAC=<file.flac>`.

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
- **Host tests don't catch embedded stack overflows** unless they measure
  stack use explicitly (see `host/stack_check`).
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
- [x] Raise PCM ring to 64 KB and re-measure 24/96 fill levels (84–98%)
- [x] Measure decoder load: now logged every 5 s (decode %, SD %, x realtime)
- [ ] Try a faster SD SPI clock (currently 20 MHz) and compare SD %
- [ ] Real hi-res source material (e.g. 2L test bench) vs upsampled files
- [ ] PCM5102A on the external I2S port: stereo, 24-bit output
- [ ] MP3 via minimp3
- [ ] SD read latency worst case (fragmented card), Wi-Fi active during playback
