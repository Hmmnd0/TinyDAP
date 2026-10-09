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
| VIN / VCC | **5V** | The board has its own 3.3 V regulator |
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
| D1 | GPIO **42** |
| DAT2 | GPIO **2** |
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
          5V  ── DAC VIN                   GND
          GND ── GND rail                  GND
```

1. **Breadboards:** the DevKitC is too wide for one breadboard to leave
   room on both sides. Put two breadboards side by side and straddle the
   board across the join, so each header row has free holes next to it.
2. **Power rails:** run **3V3** to one rail and **GND** to the other. The
   OLED, SD breakout, and DAC jumpers take power from there; the DAC's VIN
   goes to **5V** directly.
3. **Placement:** put the DAC and OLED on the J1 side, the SD breakout on
   the J3 side next to GPIO 39–42, and the buttons along the free edge of
   the J1 board.
4. **Before powering:** check that nothing connects to 5 V except the DAC's
   VIN, and that 3V and VCC on the SD breakout and OLED go to 3.3 V.

## Bring-up order

Connect and test one part at a time:

1. DevKit alone: flash over the **USB** port (not UART); logs appear.
2. PCM5102A: test tone in stereo.
3. Buttons.
4. OLED: browser UI.
5. microSD: 4-bit SDIO, then full playback.

The firmware needs a DevKitC board target with these pins before step 1.
