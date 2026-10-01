# Tiny Lossless ESP32-S3 Digital Audio Player

## Project Design, Prototype Plan, Firmware Architecture, and PCB Roadmap

**Status:** Prototype / Rev A planning\
**Primary goal:** Build a very small battery-powered digital audio
player capable of true local lossless playback from microSD, with a
monochrome OLED, physical controls, high-quality wired headphone output,
and room for later wireless features.

------------------------------------------------------------------------

## 1. Project concept

The device is a purpose-built miniature digital audio player (DAP),
roughly inspired by the physical scale of an iPod Shuffle but with a
display and removable storage.

The core design goals are:

-   Local FLAC playback from microSD
-   MP3 and other common compressed formats as secondary formats
-   High-quality wired 3.5 mm headphone output
-   0.96-inch 128 × 64 monochrome OLED
-   Physical playback controls
-   USB-C charging/programming/file-management capability
-   Wi-Fi and Bluetooth LE capability through the ESP32-S3
-   Small LiPo battery
-   Eventually a custom four-layer PCB
-   Firmware that can decode audio continuously while reading from
    storage and feeding an audio output
-   Architecture that can later support a second digital audio
    destination such as a dedicated Bluetooth-audio transmitter

The prototype will be built from development boards and breakout modules
first. Once the audio architecture and firmware are proven, the same
functional blocks can be condensed into raw ICs on the final PCB.

------------------------------------------------------------------------

## 2. MCU

### Final PCB target

**ESP32-S3-PICO-1**

The preferred family is the ESP32-S3-PICO-1 because it combines the
ESP32-S3 processor and supporting components in a very small 7 × 7 mm
SiP.

For readily available DigiKey sourcing, the practical part is:

**ESP32-S3-PICO-1-N8R8** - 8 MB flash - 8 MB PSRAM - Dual-core
ESP32-S3 - Up to 240 MHz - Wi-Fi - Bluetooth LE - Native USB - I²S -
SPI - SD/MMC - I²C - UART - Approx. 7 × 7 mm package - DigiKey one-off
price observed: about \$6.10--\$6.25

**Preferred production MCU:** ESP32-S3-PICO-1-N8R8 --- 8 MB flash + 8 MB
PSRAM in the 7 × 7 mm PICO-1 package. The N8R2 variant should be
retained as a sourcing fallback where electrically/package compatible,
but it is no longer the primary TinyDAP target.

### Why N8R8 is now preferred

TinyDAP does not require 8 MB of PSRAM merely to decode FLAC; EMBER's
Cardputer-Adv implementation demonstrates that useful FLAC playback is
possible without PSRAM. The larger N8R8 configuration is instead chosen
to provide substantial headroom for SD read-ahead, compressed-audio
buffers, metadata, library indexing, playlists, networking, future UI
features, and experimentation without increasing the PICO-1 PCB
footprint.

Latency-sensitive structures and I2S DMA buffers should still remain in
internal SRAM. PSRAM should primarily hold larger, less latency-critical
data structures.

DigiKey:
https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-PICO-1-N8R8/21264372

### PSRAM

PSRAM is not strictly required to decode FLAC, but it is desirable.

The audio player should use internal SRAM for timing-critical I²S/DMA
buffers and PSRAM for larger, less timing-sensitive allocations such as:

-   File read-ahead buffers
-   FLAC decoder working memory where appropriate
-   Metadata
-   Library indexing
-   UI assets
-   Network buffers
-   Future web interface
-   Future album-art decoding/caching
-   Future wireless features

Even 2 MB (N8R2) would be enough for basic lossless playback, which is
why N8R2 remains a viable sourcing fallback. TinyDAP targets the N8R8's
8 MB for the headroom described above, not because playback requires it.

### Prototype MCU board

Use an **ESP32-S3 development board with PSRAM** rather than attempting
to breadboard the PICO SiP.

Good prototype choices include:

-   Espressif ESP32-S3-DevKitC-1 variants with PSRAM
-   Adafruit QT Py ESP32-S3 with 8 MB PSRAM for a smaller breadboard
    setup

Adafruit QT Py ESP32-S3 product family:
https://www.adafruit.com/product/5700

The prototype does not have to use the exact PICO package. What matters
is testing the same ESP32-S3 CPU architecture, PSRAM behavior, I²S,
storage, display, USB, and firmware before shrinking it onto the PICO
SiP.

------------------------------------------------------------------------

## 3. Audio architecture

The fundamental local-playback pipeline is:

``` text
microSD
   |
   | FLAC / MP3 / WAV file data
   v
ESP32-S3
   |
   | decoder
   v
PCM ring buffer
   |
   v
I2S / DMA
   |
   v
DAC + headphone amplifier
   |
   v
3.5 mm headphones
```

The ESP32 does **not** decode an entire song into RAM.

Playback is a continuous pipeline:

1.  Read compressed blocks from microSD.
2.  Decode a small portion.
3.  Place PCM samples into buffers.
4.  DMA sends PCM to the I²S peripheral.
5.  The DAC converts the PCM stream to analog audio.
6.  Repeat while the song is playing.

This allows file reading, decoding, UI operation, and audio output to
occur concurrently.

------------------------------------------------------------------------

## 4. Audio bandwidth

CD-quality stereo PCM:

``` text
44,100 samples/sec × 16 bits × 2 channels
= 1,411,200 bits/sec
≈ 176.4 KB/sec
```

24-bit / 96 kHz stereo:

``` text
96,000 × 24 × 2
= 4,608,000 bits/sec
≈ 576 KB/sec
```

These data rates are reasonable for the ESP32-S3, microSD, and I²S/DMA
architecture.

------------------------------------------------------------------------

## 5. DAC / headphone amplifier

### Target device: Cirrus Logic CS43131

The CS43131 is the current high-end target for the final PCB.

Key specifications:

-   32-bit DAC
-   Up to 384 kHz PCM
-   I²S input
-   DSD support
-   Integrated headphone driver
-   Up to 2 Vrms output
-   Approx. 30 mW/channel into 32 ohms
-   130 dB dynamic range
-   THD+N around -115 dB
-   Integrated impedance detection
-   Integrated PLL
-   Low-power/high-performance operating modes

Cirrus Logic: https://www.cirrus.com/products/cs43131/

Datasheet:
https://statics.cirrus.com/pubs/proDatasheet/CS43131_DS1155F2.pdf

### Raw IC choices

#### CS43131-CWZR

-   42-WLCSP
-   Approx. 2.71 × 3.18 mm
-   Best option when minimum final PCB area matters
-   Difficult to hand assemble
-   DigiKey price observed September 2026: **\$18.35 each**

DigiKey:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CWZR/7430364

#### CS43131-CNZ / CNZR

-   40-QFN
-   Approx. 5 × 5 mm
-   Much friendlier for prototype/custom-carrier PCB work
-   DigiKey price observed: **\$18.52 each**

DigiKey:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CNZ/7388594

### Official CS43131 evaluation board

**Cirrus Logic CDB43131K**

This is the real official evaluation platform and provides extensive
configuration and measurement capability.

Unfortunately, it is not economical for this project.

DigiKey price observed September 2026:

**\$1,248.75**

DigiKey:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CDB43131K/9178161

Cirrus provides the evaluation-board documentation, reference
schematic/layout, and reference BOM:

https://www.cirrus.com/products/cs43131/

CDB43131 kit manual:
https://statics.cirrus.com/pubs/rdDatasheet/CDB43131-GBK_DS1155V2DB1.pdf

### Important prototype issue

There does not appear to be a widely distributed, inexpensive,
mainstream CS43131 breakout board comparable to common PCM5102A or
MAX98357 modules.

Therefore the recommended development process is:

**Stage 1:** Validate the entire ESP32 audio pipeline with an
inexpensive I²S DAC development module.

**Stage 2:** Validate the CS43131 itself using either the official
board, if access becomes practical, or a small custom CS43131-QFN
carrier PCB based closely on the Cirrus reference design.

**Stage 3:** Integrate the proven CS43131 circuit into the final TinyDAP
PCB.

This separates firmware risk from difficult mixed-signal PCB risk.

------------------------------------------------------------------------

## 6. Cheap prototype DAC

For initial breadboard firmware development, use a readily available
**PCM5102A I²S DAC breakout** or similar I²S DAC module.

The purpose is not to establish the final audio quality. It is to prove:

-   microSD reading
-   FLAC decoding
-   I²S clock generation
-   sample-rate changes
-   buffering
-   gapless playback
-   UI interaction while playing
-   file navigation
-   CPU load
-   PSRAM strategy
-   underrun behavior

Once those work, replace the prototype DAC path with the CS43131.

A PCM5102A module generally produces line-level output rather than being
a complete substitute for the CS43131 headphone driver, so an external
headphone amplifier may be needed for actual headphone testing.

------------------------------------------------------------------------

## 7. CS43131 control requirements

The CS43131 is more involved than a basic three-wire I²S DAC.

The final circuit must account for:

-   I²S audio data
-   I²C control
-   1.8 V digital/interface requirements
-   Proper clocking/MCLK strategy
-   Analog supply quality
-   Charge-pump components
-   Headphone output routing
-   Reset/control sequencing
-   Appropriate level translation where required
-   Very careful decoupling
-   Low-noise PCB layout

This is why the Cirrus reference schematic and layout should be treated
as the starting point rather than designing the analog section from
scratch.

------------------------------------------------------------------------

## 8. Display

### Final raw display

Target:

**0.96-inch 128 × 64 monochrome white OLED**

Preferred characteristics:

-   128 × 64 pixels
-   SSD1306-compatible controller
-   SPI interface preferred for the final device
-   Bare COG panel rather than a large breakout PCB
-   White monochrome
-   Approx. 1.26 mm display thickness

A representative panel discussed for the project is the Winstar
0.96-inch 128 × 64 OLED family.

The higher-resolution 128 × 64 display is preferred over the earlier 64
× 48 idea because:

-   8,192 pixels instead of 3,072
-   Better text rendering
-   Better menu layout
-   Better metadata display
-   Still only a 1 KB monochrome framebuffer

Framebuffer calculation:

``` text
128 × 64 / 8 = 1,024 bytes
```

The display therefore has essentially negligible RAM impact.

### Prototype display

Use a **0.96-inch 128 × 64 SSD1306 OLED development module that supports
4-wire SPI**.

The prototype should use **SPI from the beginning** so the display bus,
pin usage, driver configuration, update timing, and firmware
architecture match the intended production design as closely as
practical.

The Winstar **WEA012864D-01** development PCB supports SPI as an option
and is a good direct prototype choice when an SPI-configured unit can be
sourced. Because that exact configuration is not commonly stocked by
U.S. distributors, a reputable generic 0.96-inch 128 × 64 SSD1306
SPI-capable module is an acceptable and easier-to-source substitute.

Do **not** intentionally choose an I²C-only OLED for the TinyDAP
prototype. I²C would function, but it would test a different display bus
than the one planned for the final player.

Prototype-to-production path:

``` text
ESP32-S3 Dev Board
       |
   4-wire SPI
       |
       v
0.96" 128×64 SSD1306 SPI development module
       |
       | same controller / resolution / SPI architecture
       v
Winstar WEO012864D bare COG OLED on final TinyDAP PCB
```

------------------------------------------------------------------------

## 9. Storage

### Final PCB

Use a low-profile microSD socket connected to the ESP32-S3 using
**SDMMC**, preferably four-bit mode if the pin budget permits.

Previously selected candidate:

**GCT MEM2075-00-140-01-A**

DigiKey:
https://www.digikey.com/en/products/detail/gct/MEM2075-00-140-01-A/9859614

### Prototype

Use a microSD breakout.

Prefer a 3.3 V breakout that exposes the SD bus if possible. SPI mode is
perfectly adequate for early firmware testing, although final hardware
should test SDMMC before PCB freeze.

The firmware/storage layer should eventually be tested under:

-   FLAC playback
-   UI navigation
-   Large directory scans
-   Simultaneous metadata reading
-   Wi-Fi activity
-   High-resolution audio
-   Worst-case fragmented SD card access

------------------------------------------------------------------------

## 10. Audio decoder software

### Fastest bring-up: ESP32-audioI2S

Repository: https://github.com/schreibfaul1/ESP32-audioI2S

This library is unusually well matched to the project because it already
supports:

-   ESP32-S3
-   PSRAM
-   I²S
-   SD storage
-   FLAC
-   MP3
-   AAC/M4A
-   WAV
-   Opus
-   Vorbis
-   Multiple PCM sample widths

The initial proof-of-concept can therefore be:

``` text
microSD -> ESP32-audioI2S -> I2S -> prototype DAC
```

This gets audible FLAC playback working before writing a custom audio
engine.

### Long-term option: dr_flac

Repository: https://github.com/mackron/dr_libs

`dr_flac` is attractive for the production firmware because it allows
the project to own the PCM pipeline directly.

Architecture:

``` text
microSD
   |
   v
dr_flac
   |
   v
PCM ring buffer
   |
   +------> wired I2S DAC
   |
   +------> future Bluetooth audio subsystem
```

Owning the PCM stream makes future features easier:

-   simultaneous outputs
-   EQ
-   ReplayGain
-   visualizers
-   gapless playback
-   crossfade
-   resampling
-   Bluetooth transmission
-   audio analysis
-   volume processing

### MP3

A small MP3 decoder such as minimp3 could eventually be paired with
dr_flac if the project moves away from the all-in-one ESP32-audioI2S
framework.

------------------------------------------------------------------------

## 11. Concurrency

The ESP32-S3 can decode audio while continuing to read/stream data.

A possible task arrangement is:

``` text
CORE 0
------
Filesystem
SD reads
UI
Buttons
Library management
Network tasks

CORE 1
------
FLAC decoding
PCM processing
Audio scheduling

HARDWARE / DMA
--------------
I2S transfer to DAC
```

The exact task placement should be determined experimentally. The
important principle is that I²S DMA should continuously consume PCM
while the CPU fills buffers ahead of it.

------------------------------------------------------------------------

## 12. Bluetooth

### ESP32-S3 limitation

The ESP32-S3 provides **Bluetooth LE**, not Bluetooth Classic.

Traditional Bluetooth A2DP headphone audio commonly relies on Bluetooth
Classic, so the S3 cannot simply behave like a conventional A2DP source
using the same approach as the original ESP32.

### Lossless Bluetooth

Ordinary Bluetooth audio codecs such as SBC, AAC, conventional aptX,
LDAC, and LC3 are lossy.

Qualcomm aptX Lossless can provide CD-quality 16-bit/44.1-kHz lossless
operation under suitable link conditions, but it belongs to Qualcomm's
ecosystem and requires compatible transmitter and receiver hardware.

For TinyDAP, the cleanest architecture is therefore:

``` text
                         +--> CS43131 --> 3.5 mm headphones
                         |
microSD --> S3 --> PCM --+
                         |
                         +--> future Bluetooth-audio SoC --> wireless headphones
```

The ESP32 would decode the FLAC only once.

The resulting PCM could then be consumed by both destinations.

A future dedicated Bluetooth audio chip could receive PCM over I²S and
handle Bluetooth codec/profile/radio work independently.

### Rev A recommendation

Do not make Bluetooth audio a requirement for the first prototype.

Instead:

-   Preserve spare I²S-capable GPIOs.
-   Consider exposing an I²S expansion header/test pads.
-   Prove local wired playback first.
-   Add a Bluetooth-audio subsystem after the basic DAP is stable.

The S3's own BLE can still be useful for control/configuration even if
it is not used for headphone audio.

------------------------------------------------------------------------

## 13. USB-C

### Final PCB

USB-C should connect to the ESP32-S3 native USB interface.

Goals:

-   Firmware flashing
-   Debugging
-   Charging
-   Potential USB mass-storage/file-management features
-   Potential USB audio experimentation

Previously selected connector candidate:

**GCT USB4105-GF-A-060**

DigiKey:
https://www.digikey.com/en/products/detail/gct/USB4105-GF-A-060/14559043

The final board should avoid a separate USB-to-UART IC unless testing
demonstrates a compelling reason to add one.

------------------------------------------------------------------------

## 14. Battery and power

Target battery:

-   Single-cell LiPo
-   Approximately 400--600 mAh
-   Flat pouch cell
-   Positioned behind the PCB

### Charger candidate

**Texas Instruments BQ25185**

Small single-cell battery charger suitable for a compact battery-powered
product.

DigiKey:
https://www.digikey.com/en/products/detail/texas-instruments/BQ25185DLHR/21769368

### Main 3.3 V regulator candidate

**Texas Instruments TPS63031**

Fixed 3.3 V buck-boost regulator.

This allows the digital 3.3 V rail to remain regulated across much of
the LiPo discharge curve.

DigiKey:
https://www.digikey.com/en/products/detail/texas-instruments/TPS63031DSKR/2048021

### Audio power

The CS43131 requires its own carefully designed supply arrangement,
including 1.8 V domains.

The production PCB should use low-noise audio rails and follow Cirrus's
reference design closely.

Do not assume the main 3.3 V digital regulator alone is sufficient for
every CS43131 supply pin.

------------------------------------------------------------------------

## 15. Controls

Planned physical controls:

-   Play/pause
-   Previous
-   Next
-   Volume up
-   Volume down

Optional:

-   Dedicated power/hold control

Final PCB should use tiny side-actuated SMT tactile switches.

Prototype can use ordinary breadboard tact switches.

Previously considered final switch:

**GCT SWT0005-015516SSA**

DigiKey:
https://www.digikey.com/en/products/detail/gct/SWT0005-015516SSA/28022338

------------------------------------------------------------------------

## 16. Headphone jack

Final target:

**3.5 mm TRS stereo jack**

Previously considered:

**Same Sky / CUI SJ-43516-SMT-TR**

DigiKey:
https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/SJ-43516-SMT-TR/669721

The CS43131 contains the headphone-driving stage, so the final design
does not inherently require a separate conventional headphone amplifier.

------------------------------------------------------------------------

# 17. Development Stage 0: M5Stack Cardputer-Adv

The already-owned **M5Stack Cardputer-Adv (K132-ADV)** should serve as
TinyDAP **Development Stage 0**. It is not the final hardware target,
but it provides an ESP32-S3-family platform with built-in microSD,
display, controls, battery power, audio codec, headphone output, USB-C,
and ESP-IDF support.

This lets development begin with the actual ESP32/FreeRTOS environment
before the dedicated TinyDAP breadboard is assembled.

## Stage 0 audio path

``` text
Cardputer-Adv microSD
        |
        v
FLAC / MP3 / WAV
        |
        v
ESP32-S3
        |
        +--> filesystem / metadata
        |
        +--> decoder
                |
                v
           PCM buffers
                |
                v
            I2S + DMA
                |
                v
              ES8311
                |
                v
        3.5 mm headphones
```

The Cardputer-Adv ES8311 subsystem uses I2C for codec control and I2S
for audio. Documented connections include GPIO 8/9 for I2C control and
GPIO 41/43/46/42 for the audio interface. Its built-in microSD uses GPIO
12/14/40/39. These assignments should be verified against the current
M5Stack schematic before committing firmware.

The built-in 240 × 135 display can temporarily show metadata, sample
rate, codec, playback position, buffer fill, memory use, CPU/task
diagnostics, and underrun counts. The keyboard can stand in for
play/pause, previous, next, volume, menu, and debug controls.

## EMBER reference implementation

**EMBER** by HorseyofCoursey should be treated as an important Stage 0
reference implementation:

https://github.com/HorseyofCoursey/EMBER

EMBER is MIT-licensed music-player firmware for the Cardputer-Adv. It
already demonstrates several capabilities directly relevant to TinyDAP,
including local microSD playback, FLAC/MP3/WAV/AAC decoding, metadata
and file browsing, album art, visualization, and network music
streaming.

Most importantly for TinyDAP, EMBER also demonstrates an **external I2S
DAC path** from the Cardputer-Adv. Its documented external DAC support
uses the Cardputer's second I2S peripheral, providing a useful working
reference when bringing up our PCM5102A prototype and, later, the custom
CS43131 carrier.

EMBER should therefore be used to:

-   Confirm known-working Cardputer-Adv audio and storage behavior.
-   Study its decoder, buffering, filesystem, metadata, UI, and I2S
    architecture.
-   Accelerate external-DAC bring-up rather than proving every basic
    ESP32-S3 audio concept from zero.
-   Provide a comparison point when our own FreeRTOS/audio architecture
    behaves unexpectedly.
-   Identify Cardputer-Adv resource constraints before moving to the
    TinyDAP hardware.

Because EMBER is MIT licensed, code may be reused where appropriate as
long as its license requirements and attribution are preserved.

EMBER is **not** the TinyDAP architecture itself. TinyDAP remains a
purpose-built player with a custom ESP32-S3 PCB, Winstar SPI OLED,
dedicated controls, custom LiPo/power circuitry, microSD hardware, and
the CS43131 DAC/headphone path. Code adopted from EMBER should be kept
portable and separated from Cardputer-specific assumptions whenever
practical.

A useful progression is:

``` text
EMBER on Cardputer-Adv
        |
        | known-working local playback
        v
Cardputer-Adv external I2S
        |
        +--> PCM5102A prototype DAC
        |
        v
our TinyDAP firmware architecture
        |
        v
CS43131-CNZ carrier
        |
        v
final TinyDAP hardware
```

This changes the purpose of Stage 0 slightly: rather than spending time
proving that an ESP32-S3 can function as a capable music player, Stage 0
should use EMBER as a reference while concentrating on the architecture
and hardware interfaces that TinyDAP needs to own.

## Stage 0 goals

Use the Cardputer-Adv to prove:

-   ESP-IDF build/flash workflow
-   FreeRTOS task architecture
-   microSD filesystem access
-   FLAC, MP3, and WAV decoding
-   PCM buffering
-   I2S + DMA playback
-   ES8311 control over I2C
-   metadata parsing
-   playlists/directory handling
-   playback state machine and track changes
-   gapless-playback experiments
-   underrun detection
-   long-duration playback stability
-   memory and task-stack measurements
-   UI architecture
-   input/event architecture

## What it does not replace

The Cardputer-Adv does **not** validate the final 128 × 64 SSD1306 over
4-wire SPI, the dedicated microSD implementation, PCM5102A external
bring-up, CS43131 circuitry, final power/charging system, final USB-C
circuit, final physical controls, or final PCB layout.

Those remain dedicated prototype milestones.

## Revised development sequence

``` text
STAGE 0 — Cardputer-Adv
microSD -> decoder -> PCM -> I2S -> ES8311 -> headphones
        |
        v
STAGE 1 — Dedicated ESP32-S3 breadboard
microSD + SPI SSD1306 + buttons + PCM5102A
        |
        v
STAGE 2 — Custom CS43131-CNZ carrier
I2S + I2C + clocking + power + headphone output
        |
        v
STAGE 3 — Battery / charger / regulator validation
        |
        v
STAGE 4 — TinyDAP Rev A PCB
ESP32-S3-PICO + WEO012864D + CS43131 + microSD + USB-C + LiPo
```

The optional Raspberry Pi remains useful as an independent Linux/I2S
reference source, especially when debugging the CS43131 carrier.

------------------------------------------------------------------------

# 18. ESP-IDF / FreeRTOS real-time architecture

TinyDAP should be designed around the ESP32-S3's **FreeRTOS** runtime
rather than as one large blocking playback loop. ESP-IDF uses FreeRTOS,
which lets storage, decoding, audio output, controls, display work, and
background services run as coordinated tasks.

The central rule is that **audio output must never wait on slow UI,
filesystem, metadata, or network work**.

A practical starting architecture is:

``` text
microSD / SDMMC
      |
      v
[Storage / read-ahead task]
      |
 compressed-data buffer
      |
      v
[Decoder task]
      |
    PCM ring buffer
      |
      v
[I²S + DMA]
      |
      v
     DAC
```

Other lower-priority work runs alongside that pipeline:

``` text
Buttons ----> input/event task -----+
                                    |
OLED <------ UI/display task <------+
                                    |
Metadata / library task ------------+
                                    |
Wi-Fi / BLE services (when enabled)-+
```

## Suggested task responsibilities

### Audio / I²S path

I²S DMA should continuously consume PCM buffers. DMA buffers and other
latency-critical structures should live in **internal SRAM**, not PSRAM.

The audio path gets the highest practical priority because an underrun
is immediately audible.

### Decoder task

The decoder converts FLAC/MP3/etc. into PCM and keeps the PCM ring
buffer comfortably ahead of the DMA consumer.

`dr_flac` is a strong long-term candidate for the FLAC path. Initial
bring-up can use ESP32-audioI2S.

### Storage / read-ahead task

The storage task reads compressed data from microSD in larger chunks so
brief SD-card latency spikes do not starve the decoder.

Large read-ahead caches are a good use of **PSRAM**.

### UI task

The OLED task updates the SSD1306 framebuffer over SPI without blocking
audio. Display refreshes do not need audio-level priority.

### Input task

Buttons should generate events rather than directly performing expensive
player operations from GPIO/interrupt context.

### Metadata / library task

Tag parsing, directory scans, sorting, and library indexing should run
at lower priority and yield readily to playback work.

### Networking

Wi-Fi/BLE work should remain lower priority than playback. If radio
features are unnecessary during local playback, firmware can disable or
reduce them to save power and reduce RF/noise activity.

## Inter-task communication

Prefer explicit FreeRTOS primitives rather than shared mutable state:

-   Queues for button/player commands
-   Ring buffers or stream buffers for compressed/PCM data
-   Event groups for player state
-   Mutexes only where genuinely required
-   Task notifications for lightweight one-to-one signaling

Avoid holding locks in the audio path.

## Dual-core starting point

A reasonable initial ESP32-S3 allocation is:

``` text
Core 0
  storage / filesystem
  UI
  buttons
  metadata
  networking

Core 1
  audio decoder
  audio pipeline coordination

I²S peripheral + DMA
  continuous PCM transfer to DAC
```

This is a starting point, not a requirement. Actual core affinity and
priorities should be determined from measurements during prototype
stress testing.

## What to measure

During development, log or expose:

-   PCM ring-buffer fill level
-   Compressed read-ahead fill level
-   I²S underrun count
-   Worst-case SD read latency
-   Decoder execution time
-   Internal SRAM usage
-   PSRAM usage
-   Per-task stack high-water marks
-   CPU utilization
-   Track-change latency

The system should be stress-tested while rapidly changing tracks,
refreshing the OLED, reading metadata, and optionally using Wi-Fi/BLE.
If those operations cannot cause an audible underrun, the real-time
architecture is doing its job.

------------------------------------------------------------------------

# 19. Complete prototype shopping list

**Already owned:** M5Stack Cardputer-Adv for Stage 0 development; no
purchase is required for this stage.

This is the **bench-ready prototype BOM**. It is intentionally different
from the final production BOM. The goal is to have everything needed to
prove storage, decoding, I²S audio, SPI display, controls, USB
development, and later CS43131 integration without making the difficult
final DAC circuit the first dependency.

## A. Required for Prototype Stage 1: complete working player

### MCU / development

-   **1× ESP32-S3 development board with PSRAM**
    -   Preferred: ESP32-S3-DevKitC-1 variant with PSRAM
    -   Compact alternative: Adafruit QT Py ESP32-S3 with 8 MB PSRAM
-   **1× USB data cable** appropriate for the selected ESP32-S3 board
-   **1× solderless breadboard**
-   **1 set male-to-male jumper wires**
-   **1 set male-to-female jumper wires**
-   **Breakaway 0.1-inch headers**, if any selected modules arrive
    without pins installed

The development board must expose enough GPIO for microSD, I²S audio,
SPI OLED, and five playback buttons simultaneously.

### Display

-   **1× 0.96-inch 128 × 64 SSD1306 OLED development module with 4-wire
    SPI**
-   White monochrome preferred
-   3.3 V logic compatibility preferred
-   Winstar **WEA012864D-01** is the preferred Winstar development PCB
    if an SPI-configured unit can be sourced
-   A reputable generic SSD1306 SPI module is an acceptable prototype
    substitute

Required display signals should include the equivalent of:

``` text
VCC
GND
SCLK
MOSI / SDA
CS
DC / SA0 as required by module
RST
```

Avoid buying an I²C-only module for this prototype because the final
TinyDAP display architecture is SPI.

### Storage

-   **1× microSD breakout/module**
-   Must be compatible with 3.3 V ESP32 logic
-   Prefer a module that exposes the native SD pins rather than hiding
    the card behind a separate storage controller
-   **1× quality microSD card**, approximately 16--64 GB is sufficient
    for development
-   **1× full-size SD adapter or USB card reader** if needed to load
    test files

SPI mode is acceptable for the earliest SD bring-up. Before the final
PCB is frozen, storage should also be tested using the intended ESP32-S3
SDMMC arrangement.

### Initial audio output

-   **1× PCM5102A I²S DAC breakout**
-   **1× temporary stereo headphone amplifier breakout** if the selected
    PCM5102A board is line-level only and actual headphones will be used
-   **1× pair of known-good wired headphones**, preferably an ordinary
    32-ohm pair for repeatable testing
-   **1× 3.5 mm stereo cable / adapter** if required by the chosen DAC
    or temporary headphone-amplifier board

The PCM5102A is a bring-up device only. It proves the ESP32-S3's PCM/I²S
pipeline; it is not the final TinyDAP DAC.

### Physical controls

-   **5× momentary tactile pushbuttons** minimum:
    -   Play/pause
    -   Previous
    -   Next
    -   Volume up
    -   Volume down
-   **1× additional tactile switch** recommended for experimenting with
    power/menu/hold behavior
-   **6× 10 kΩ resistors** on hand for pull-ups/pull-downs if required

ESP32 internal pulls can be used initially, but having physical
resistors available avoids blocking testing.

### Basic prototyping passives and tools

Keep a small assortment available rather than buying values one at a
time:

-   100 nF ceramic capacitors
-   1 µF and 10 µF capacitors
-   1 kΩ, 4.7 kΩ, 10 kΩ, and 100 kΩ resistors
-   2.54 mm header pins
-   Short hookup wire
-   Solder and soldering iron for modules that need headers installed
-   Multimeter
-   Optional but useful: inexpensive logic analyzer for SPI, I²C, I²S,
    and SD debugging
-   Optional but useful: oscilloscope for clocks, power rails, and later
    CS43131 work

### Test media

Prepare a known-good test set containing:

-   FLAC 16-bit / 44.1 kHz
-   FLAC 24-bit / 44.1 kHz
-   FLAC 24-bit / 48 kHz
-   FLAC 24-bit / 96 kHz
-   MP3 at several bitrates
-   WAV reference files
-   Short tracks for rapid track-change testing
-   Long tracks for soak/underrun testing
-   Tracks with useful metadata/tags for OLED testing

### Stage 1 power

Use the ESP32-S3 development board's **USB power** for the first player
prototype.

Do not add LiPo charging, the final buck-boost converter, or low-noise
audio power circuitry until SD playback, decoding, I²S output, OLED, and
buttons are stable.

## B. Prototype Stage 2: CS43131 validation

Do **not** buy the roughly \$1,200 official Cirrus CDB43131K evaluation
kit solely for this project.

After Stage 1 works, build a small dedicated **CS43131
carrier/development PCB** using the easier-to-assemble QFN version and
the Cirrus reference design.

Parts/categories required for that board are:

-   **1--3× CS43131-CNZ** 5 × 5 mm QFN devices; a spare is sensible for
    assembly risk
-   Small 4-layer carrier PCB
-   3.5 mm TRS headphone jack
-   Appropriate low-noise 1.8 V supply circuitry
-   Any required 3.3 V ↔ 1.8 V logic translation
-   Charge-pump capacitors and all CS43131 decoupling capacitors
    specified by Cirrus
-   Required clock/MCLK circuitry determined from the final clocking
    strategy
-   Reset/control components
-   I²S header/test pads
-   I²C header/test pads
-   Power and ground header/test pads
-   Headphone-output protection/filter components required by the
    reference implementation
-   Test points for every important supply rail and clock
-   Optional board-mounted 3.5 mm jack so the carrier behaves as a
    self-contained TinyDAP audio subsystem

The carrier should expose at minimum:

``` text
3V3 / input power
GND
I2S BCLK
I2S LRCLK / WS
I2S DATA
MCLK if used
I2C SDA
I2C SCL
RESET / control
L/R headphone output
```

The exact CS43131 passives, regulators, level translators, and clock
parts should be selected from the current Cirrus datasheet/reference
schematic when the carrier schematic is drawn rather than guessed in
advance.

## C. Prototype Stage 3: battery and portable-power validation

Only after the USB-powered player is stable, add:

-   **1× protected 1-cell LiPo**, approximately 400--600 mAh
-   **1× BQ25185-based charger breakout/evaluation implementation** or a
    small dedicated power-test PCB
-   **1× 3.3 V buck-boost development board** representative of the
    TPS63031 architecture, or a small power-test PCB
-   JST/battery connector appropriate to the selected cell
-   Power switch or load-disconnect arrangement as required
-   USB-C power source/cable
-   Optional USB power meter for charge/current measurements

This stage should measure idle current, playback current, display-on
current, Wi-Fi current, charge behavior, battery runtime, and whether
switching-regulator noise is audible.

## D. What does NOT need to be purchased yet

The following final-production parts are **not required to begin the
breadboard prototype**:

-   ESP32-S3-PICO-1 raw SiP
-   CS43131-CWZR WLCSP
-   Bare Winstar WEO012864D COG panel
-   Final microSD socket
-   Final USB-C receptacle
-   Final SMT side buttons
-   Final SMT headphone jack
-   Final BQ25185 IC
-   Final TPS63031 IC
-   Final enclosure

Those become relevant when the prototype has established the electrical
and firmware requirements for Rev A.

------------------------------------------------------------------------

# 20. Optional Raspberry Pi reference/test platform

A Raspberry Pi is **not** the primary TinyDAP development target. The
ESP32-S3 remains the production processor and the platform on which
real-time playback, storage, UI, and power behavior must ultimately be
validated.

A Raspberry Pi is nevertheless useful as an **optional known-good Linux
audio test bench**, especially when the custom CS43131 carrier is built.

Recommended inexpensive option:

-   **Raspberry Pi Zero 2 W**
-   microSD card for Raspberry Pi OS
-   GPIO header if the selected Pi does not have one installed
-   Female jumper wires / appropriate GPIO breakout
-   Optional logic analyzer and/or oscilloscope

Useful roles for the Pi include:

1.  Generating known-good I²S audio for testing the CS43131 carrier
    independently of ESP32 firmware.
2.  Comparing I²S clocks and data against the ESP32-S3 output.
3.  Exercising 44.1, 48, and 96 kHz PCM while observing BCLK, LRCLK/WS,
    DATA, and MCLK where applicable.
4.  Testing the decoder/player core on Linux without tying it to the
    embedded hardware abstraction.
5.  Providing a clean debugging boundary:

``` text
Pi -> CS43131 fails
    = investigate CS43131 carrier, power, clocking, control, or layout

Pi -> CS43131 works
ESP32 -> CS43131 fails
    = investigate ESP32 I²S/I²C configuration or firmware
```

Where practical, TinyDAP software should keep portable player logic
separate from platform-specific hardware code:

``` text
tinyDAP/
├── codecs/
├── player/
│   ├── decoder
│   ├── buffering
│   ├── metadata
│   └── playlist
└── platform/
    ├── esp32/
    │   ├── audio_i2s
    │   ├── storage_sd
    │   └── display_spi
    └── linux/
        ├── audio_alsa
        └── storage_linux
```

This can allow decoder, metadata, playlist, and buffering logic to be
tested under Linux while preserving ESP-IDF/FreeRTOS-specific code for
the actual player.

The Raspberry Pi should therefore be treated as **optional development
equipment**, not as a TinyDAP component and not as a substitute for
ESP32-S3 testing.

------------------------------------------------------------------------

# 21. Final raw-component BOM

The following is the current intended direction, not yet a frozen
production BOM.

  -----------------------------------------------------------------------------
  Function             Raw component          Current role
  -------------------- ---------------------- ---------------------------------
  MCU                  ESP32-S3-PICO-1-N8R8   Main processor, Wi-Fi/BLE, USB,
                                              I²S, SDMMC

  DAC/headphone        CS43131-CWZR           Final compact high-fidelity
                                              DAC/headphone driver

  Prototype-friendly   CS43131-CNZ            QFN alternative for
  DAC package                                 carrier/prototype PCB

  OLED                 0.96" 128×64 SSD1306   User interface
                       bare COG OLED          

  Storage              GCT                    microSD socket
                       MEM2075-00-140-01-A    

  USB                  GCT USB4105-GF-A-060   USB-C

  Charger              TI BQ25185DLHR         LiPo charging

  Main regulator       TI TPS63031DSKR        3.3 V buck-boost

  Headphone jack       SJ-43516-SMT-TR        3.5 mm stereo output

  Buttons              GCT SWT0005-015516SSA  Side controls

  Battery              400--600 mAh 1S LiPo   Portable power

  Audio regulators     TBD low-noise 1.8 V    CS43131 supplies
                       rail(s)                

  Level shifting       TBD                    3.3 V ESP32 ↔ 1.8 V CS43131 where
                                              required

  ESD                  TBD                    USB/headphone/user-accessible
                                              protection

  Passives             0201/0402/0603 as      Decoupling/filtering/bias/power
                       appropriate            

  Inductor             Per TPS63031 design    Buck-boost power stage
  -----------------------------------------------------------------------------

------------------------------------------------------------------------

# 22. Raw-component vs prototype equivalents

  ---------------------------------------------------------------------
  Final hardware                     Prototype equivalent
  ---------------------------------- ----------------------------------
  ESP32-S3-PICO-1                    ESP32-S3 DevKitC / QT Py S3

  Bare SSD1306 OLED                  SSD1306 OLED breakout

  Raw microSD socket                 microSD breakout

  CS43131                            PCM5102A breakout initially

  CS43131                            CDB43131K or custom CS43131-QFN
                                     carrier for final DAC validation

  BQ25185                            USB power initially; charger
                                     breakout/test board later

  TPS63031                           Dev-board regulator initially

  SMT tact switches                  Breadboard pushbuttons

  SMT 3.5 mm jack                    Jack on temporary DAC/headphone
                                     board

  LiPo                               USB bench power first; LiPo later
  ---------------------------------------------------------------------

------------------------------------------------------------------------

# 23. Recommended prototype sequence

## Phase 1 --- Make sound

Connect:

``` text
ESP32-S3 Dev Board
      |
      +--> microSD breakout
      |
      +--> PCM5102A I2S DAC
```

Firmware goal:

-   Mount microSD.
-   Open one FLAC file.
-   Decode it.
-   Send PCM through I²S.
-   Obtain clean uninterrupted audio.

No OLED, battery, Wi-Fi, or menu system yet.

## Phase 2 --- Stress the decoder

Test:

-   16/44.1 FLAC
-   24-bit FLAC
-   48 kHz
-   96 kHz
-   MP3
-   WAV
-   Long tracks
-   Rapid track changes

Measure:

-   CPU usage
-   PSRAM usage
-   Internal SRAM usage
-   Buffer underruns
-   SD read latency

## Phase 3 --- Add display

Add SSD1306 OLED.

Display:

-   Track
-   Artist
-   Album
-   Elapsed time
-   Duration
-   Volume
-   Battery eventually
-   Play/pause state

Confirm UI updates do not disturb audio.

## Phase 4 --- Add controls

Add five physical buttons and implement:

-   Play/pause
-   Next
-   Previous
-   Volume +
-   Volume -

## Phase 5 --- Build the real PCM pipeline

Decide whether to retain ESP32-audioI2S or move toward:

``` text
filesystem
   |
dr_flac
   |
PCM ring buffer
   |
audio output abstraction
   |
I2S
```

The second architecture is preferred if simultaneous outputs/DSP become
important.

## Phase 6 --- CS43131 carrier test

Before putting the CS43131 on the final player PCB:

1.  Use the 5 × 5 mm CS43131 QFN package.
2.  Build a small dedicated carrier/evaluation PCB.
3.  Follow Cirrus's reference design.
4.  Expose:
    -   I²S
    -   I²C
    -   MCLK
    -   reset
    -   power
    -   ground
    -   headphone output
5.  Connect it to the known-working ESP32 prototype.
6.  Write/test CS43131 initialization.
7.  Confirm all target sample rates.
8.  Measure noise and stability.

Once this works, shrink to the WLCSP part if the size savings justify
the assembly complexity.

## Phase 7 --- Battery system

Add:

-   LiPo
-   charger
-   power-path behavior
-   3.3 V regulation
-   audio rails
-   battery measurement
-   shutdown/sleep behavior

Test RF and audio noise while charging.

## Phase 8 --- Final PCB

Only after all previous blocks are proven should the complete player PCB
be laid out.

------------------------------------------------------------------------

# 24. Final PCB architecture

``` text
                    USB-C
                      |
                      v
              charger / power
                      |
              +-------+-------+
              |               |
             LiPo          3.3 V rail
                              |
                              v
                    ESP32-S3-PICO-1
                    /      |       \
                   /       |        \
              SDMMC       SPI       I2S
                |          |          |
             microSD      OLED        |
                                      v
                                   CS43131
                                      |
                                      v
                                  3.5 mm TRS
```

Potential future branch:

``` text
ESP32 PCM/I2S
      |
      +--> CS43131 --> wired headphones
      |
      +--> Bluetooth audio SoC --> wireless headphones
```

------------------------------------------------------------------------

# 25. PCB layout strategy

Target a **four-layer PCB**.

Suggested stack:

``` text
L1 — components + high-speed/local signals
L2 — uninterrupted ground plane
L3 — power + slow signals
L4 — components + remaining signals
```

Important layout principles:

-   Keep ESP32/RF region away from sensitive analog headphone circuitry.
-   Respect ESP32 antenna requirements.
-   Keep switching regulator currents compact.
-   Keep audio supply filtering physically close to the CS43131.
-   Use a continuous ground reference rather than casually splitting
    ground planes.
-   Keep I²S traces short and well referenced.
-   Follow Cirrus reference placement for charge-pump and analog
    components.
-   Keep microSD current spikes and clock lines away from analog output.
-   Place USB ESD close to the connector.
-   Provide test pads.

Suggested test pads:

-   GND
-   3V3
-   1V8 audio
-   battery
-   EN/reset
-   boot
-   USB D+
-   USB D-
-   I²S BCLK
-   I²S LRCLK
-   I²S DATA
-   MCLK
-   I²C SDA
-   I²C SCL

------------------------------------------------------------------------

# 26. Approximate physical target

Earlier estimates evolved as the display dimensions became clearer.

A realistic first custom PCB target is approximately:

**32 × 27 mm PCB**

Possible enclosure:

**\~34 × 29 × 10 mm**

The 0.96-inch OLED glass is one of the major footprint constraints, at
roughly 26--27 mm wide.

An aggressive later revision could potentially shrink further, but Rev A
should prioritize:

-   clean routing
-   RF clearance
-   analog isolation
-   manufacturability
-   debug access

over saving the final few millimeters.

------------------------------------------------------------------------

# 27. Firmware feature roadmap

## Minimum viable firmware

-   FAT/exFAT storage support as appropriate
-   FLAC playback
-   MP3 playback
-   WAV playback
-   Play/pause
-   Previous/next
-   Volume
-   OLED metadata
-   Folder navigation
-   Reliable resume/track switching

## Next tier

-   Gapless playback
-   ReplayGain
-   Library indexing
-   Playlist support
-   Shuffle/repeat
-   Sleep timer
-   Battery gauge
-   Persistent settings
-   USB file transfer
-   Wi-Fi file upload
-   OTA firmware updates

## Experimental

-   EQ/DSP
-   Spectrum display
-   Album art
-   Bluetooth audio transmitter subsystem
-   Simultaneous wired/wireless PCM
-   Remote control from phone
-   Network audio/streaming
-   Spotify remote/control mode

------------------------------------------------------------------------

# 28. Spotify distinction

Two very different things are often called an "ESP32 Spotify player."

### Spotify remote

The ESP32 uses Spotify's Web API to:

-   show current track
-   show artist
-   control playback
-   skip
-   pause
-   change volume

The actual Spotify audio plays somewhere else.

This is relatively practical.

### Actual Spotify endpoint

A device that receives and decodes Spotify audio itself is a much more
complicated product/licensing/software problem.

Spotify's embedded/Connect hardware ecosystem should not be assumed to
be an ordinary open ESP32 library.

For TinyDAP, Spotify should therefore remain a later optional feature.
Local lossless playback is the primary design target.

------------------------------------------------------------------------

# 29. Why prototype first

Changing from immediate PCB production to a breadboard/module prototype
is the safer development path.

The highest-risk unknowns are not whether the ESP32-S3 is theoretically
powerful enough. They are:

-   Decoder/library behavior
-   Buffer sizing
-   SD-card latency
-   24-bit sample handling
-   I²S configuration
-   CS43131 initialization
-   CS43131 clocking
-   3.3 V to 1.8 V interface details
-   Audio power quality
-   Noise from Wi-Fi and regulators
-   UI/audio concurrency
-   Battery runtime

A modular prototype allows each problem to be solved independently.

Once:

``` text
SD -> decoder -> PCM -> I2S -> audio
```

is rock solid, the hardware can be condensed confidently.

------------------------------------------------------------------------

# 30. Current design decisions

### Fairly firm

-   ESP32-S3 architecture
-   ESP32-S3-PICO-1-N8R8 final MCU (N8R2 as sourcing fallback)
-   Local microSD storage
-   FLAC as primary lossless format
-   0.96" 128×64 monochrome OLED
-   3.5 mm wired output
-   USB-C
-   Physical buttons
-   LiPo power
-   Four-layer final PCB

### Still under evaluation

-   CS43131 WLCSP vs QFN in final hardware
-   Exact OLED panel
-   Exact LiPo dimensions/capacity
-   Exact low-noise audio regulators
-   Exact level shifter
-   Exact battery fuel gauge, if any
-   ESP32-audioI2S vs custom dr_flac pipeline
-   Bluetooth audio subsystem
-   Final board dimensions

------------------------------------------------------------------------

# 31. Recommended immediate purchase

For the first test, buy only enough hardware to prove the digital audio
path:

1.  ESP32-S3 development board with PSRAM
2.  microSD breakout
3.  0.96" 128×64 SSD1306 OLED breakout
4.  PCM5102A I²S DAC breakout
5.  Breadboard
6.  Jumper wires
7.  Five tact switches
8.  Quality microSD card
9.  Headphone amp module if the selected PCM5102A board is line-level
    only

Do **not** buy the \$1,248.75 Cirrus evaluation board for the first
prototype.

Do **not** make the CS43131 the first thing that must work.

First prove:

``` text
FLAC on SD
    ->
ESP32-S3 decoder
    ->
PCM
    ->
I2S
    ->
DAC
    ->
clean audio
```

Then substitute the CS43131 into a system that is already known to work.

------------------------------------------------------------------------

# 32. Reference links

Cirrus Logic CS43131: https://www.cirrus.com/products/cs43131/

CS43131 datasheet:
https://statics.cirrus.com/pubs/proDatasheet/CS43131_DS1155F2.pdf

CDB43131 evaluation kit manual:
https://statics.cirrus.com/pubs/rdDatasheet/CDB43131-GBK_DS1155V2DB1.pdf

DigiKey CS43131 WLCSP:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CWZR/7430364

DigiKey CS43131 QFN:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CNZ/7388594

DigiKey official CS43131 evaluation board:
https://www.digikey.com/en/products/detail/cirrus-logic-inc/CDB43131K/9178161

DigiKey ESP32-S3-PICO-1-N8R8:
https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-PICO-1-N8R8/21264372

Winstar WEA012864D-01 official:
https://www.winstar.com.tw/products/oled-module/graphic-oled-display/oled-12864.html

ESP32-audioI2S: https://github.com/schreibfaul1/ESP32-audioI2S

dr_libs / dr_flac: https://github.com/mackron/dr_libs

------------------------------------------------------------------------

## Bottom line

The project is technically realistic.

The ESP32-S3 is capable of reading lossless files, decoding them
continuously, maintaining buffered PCM, driving an I²S DAC, updating a
small OLED, and responding to controls at the same time.

PSRAM gives the firmware substantial breathing room but is not what
makes basic FLAC playback possible.

The CS43131 is an excellent candidate for the final high-quality wired
audio stage, but it should be treated as a second-stage hardware
integration problem rather than the first breadboard milestone.

The prototype should first establish a known-good ESP32-S3 audio
pipeline with inexpensive modules. Once that works reliably, the CS43131
can be introduced, characterized, and ultimately integrated into a
compact custom PCB.
