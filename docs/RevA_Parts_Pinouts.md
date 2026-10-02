# Rev A Parts — Pinouts and Links

Pinouts for the Rev A BOM (write-up §21), transcribed from the
manufacturers' datasheets listed with each part. Compiled 2026-10-02.
**Check the datasheet revision before drawing symbols and footprints;**
this is a working reference, not a substitute for the source documents.

Notes marked **TinyDAP** are design consequences for this project.

---

## Findings that affect the write-up

1. **The CS43131's digital I/O is 1.8 V only** (VL = 1.66–1.94 V). Every
   VL-domain signal between it and the 3.3 V ESP32-S3 needs level
   translation: I²S (SCLK1, LRCK1, SDIN1), MCLK, and I²C (SDA, SCL).
   RESET, INT, and HP_DETECT are in the VP domain (3.0–5.25 V) and can
   connect at 3.3 V directly. §21 lists level shifting as "TBD, where
   required"; it is required.
2. **ESP32-S3-PICO-1-N8R8 is rated −40 to 65 °C ambient** (the N8R2 is
   rated to 85 °C). Fine for a pocket player, but worth noting next to the
   N8R2 fallback.
3. **The SJ-43516-SMT-TR is a 4-conductor (TRRS) jack with tip and ring
   switches**, not the "3.5 mm TRS stereo jack" the write-up describes. It
   works with ordinary TRS headphones (ring 2 and sleeve both land on the
   plug's sleeve) and its tip switch can drive the CS43131 HP_DETECT.
4. **WEO012864D comes in two FPC versions**: `WEO012864DWPP3N00006`
   (default, hot-bar soldered FPC) and `WEO012864DWPP3N00F00` (ZIF FPC
   for a connector). Rev A needs to pick one.

---

## 1. MCU — Espressif ESP32-S3-PICO-1-N8R8 (N8R2 fallback)

- Product page: https://www.espressif.com/en/products/modules/esp32-s3
- Datasheet (v1.2):
  https://www.espressif.com/sites/default/files/documentation/esp32-s3-pico-1_datasheet_en.pdf
- DigiKey N8R8: https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-PICO-1-N8R8/21264372
- DigiKey N8R2: search "ESP32-S3-PICO-1-N8R2" on digikey.com
- Package: 56-pin LGA SiP, 7 × 7 mm, plus exposed ground pad (pin 57)

| Variant | Flash | PSRAM | Ambient |
|---|---|---|---|
| N8R2 | 8 MB quad SPI | 2 MB quad SPI | −40 to 85 °C |
| N8R8 | 8 MB quad SPI | 8 MB **octal** SPI | −40 to **65** °C |

| Pin | Name | Main functions | Notes |
|---|---|---|---|
| 1 | LNA_IN | RF in/out | Antenna matching network |
| 2, 3 | VDD3P3 | Analog power | 3.3 V |
| 4 | CHIP_PU | Enable (high = on) | **Do not leave floating** (RC delay) |
| 5 | GPIO0 | RTC_GPIO0 | **Strapping: boot mode** (pull-up, 1 = SPI boot) |
| 6 | GPIO1 | TOUCH1, ADC1_CH0 | |
| 7 | GPIO2 | TOUCH2, ADC1_CH1 | |
| 8 | GPIO3 | TOUCH3, ADC1_CH2 | **Strapping: JTAG source** (floating) |
| 9 | GPIO4 | TOUCH4, ADC1_CH3 | |
| 10 | GPIO5 | TOUCH5, ADC1_CH4 | |
| 11 | GPIO6 | TOUCH6, ADC1_CH5 | |
| 12 | GPIO7 | TOUCH7, ADC1_CH6 | |
| 13 | GPIO8 | TOUCH8, ADC1_CH7 | |
| 14 | GPIO9 | TOUCH9, ADC1_CH8, FSPIHD | |
| 15 | GPIO10 | ADC1_CH9, FSPICS0 | |
| 16 | GPIO11 | ADC2_CH0, FSPID | |
| 17 | GPIO12 | ADC2_CH1, FSPICLK | |
| 18 | GPIO13 | ADC2_CH2, FSPIQ | |
| 19 | GPIO14 | ADC2_CH3, FSPIWP | |
| 20 | VDD3P3_RTC | Analog power | 3.3 V |
| 21 | XTAL_32K_P | GPIO15, U0RTS, ADC2_CH4 | Optional 32 kHz crystal |
| 22 | XTAL_32K_N | GPIO16, U0CTS, ADC2_CH5 | Optional 32 kHz crystal |
| 23 | GPIO17 | U1TXD, ADC2_CH6 | |
| 24 | GPIO18 | U1RXD, ADC2_CH7, CLK_OUT3 | |
| 25 | GPIO19 | **USB_D−**, U1RTS, ADC2_CH8 | Native USB to USB-C |
| 26 | GPIO20 | **USB_D+**, U1CTS, ADC2_CH9 | Native USB to USB-C |
| 27 | GPIO21 | | |
| 28 | SPICS1 / GPIO26 | | **Used by PSRAM on both N8R2 and N8R8** |
| 29 | VDD_SPI | Power output (from VDD3P3_RTC) | Flash/PSRAM supply |
| 30–35 | NC | | No connection |
| 36 | SPICLK_N / GPIO48 | | Supply: VDD3P3_CPU or VDD_SPI |
| 37 | SPICLK_P / GPIO47 | | Supply: VDD3P3_CPU or VDD_SPI |
| 38 | GPIO33 | SPIIO4, FSPIHD | **N8R8: used by octal PSRAM** |
| 39 | GPIO34 | SPIIO5, FSPICS0 | **N8R8: used by octal PSRAM** |
| 40 | GPIO35 | SPIIO6, FSPID | **N8R8: used by octal PSRAM** |
| 41 | GPIO36 | SPIIO7, FSPICLK | **N8R8: used by octal PSRAM** |
| 42 | GPIO37 | SPIDQS, FSPIQ | **N8R8: used by octal PSRAM** |
| 43 | GPIO38 | FSPIWP | |
| 44 | MTCK / GPIO39 | JTAG TCK, CLK_OUT3 | |
| 45 | MTDO / GPIO40 | JTAG TDO, CLK_OUT2 | |
| 46 | VDD3P3_CPU | Digital power for CPU I/O | 3.3 V |
| 47 | MTDI / GPIO41 | JTAG TDI, CLK_OUT1 | |
| 48 | MTMS / GPIO42 | JTAG TMS | |
| 49 | U0TXD / GPIO43 | UART0 TX (boot log) | |
| 50 | U0RXD / GPIO44 | UART0 RX | |
| 51 | GPIO45 | | **Strapping: VDD_SPI voltage** (pull-down, 0 = 3.3 V) |
| 52 | GPIO46 | | **Strapping: boot mode, ROM log** (pull-down) |
| 53, 54 | NC | | No connection |
| 55, 56 | VDDA | Analog power | 3.3 V |
| 57 | GND | Exposed pad | Ground |

Strapping defaults: GPIO0 pull-up (1), GPIO3 floating, GPIO45 pull-down (0),
GPIO46 pull-down (0). Any GPIO can carry I²S, SPI, or I²C through the GPIO
matrix; SDMMC can also be routed through the matrix.

**TinyDAP:** with N8R8, GPIO26 and GPIO33–37 are unavailable. Keep GPIO45
low at boot (3.3 V flash). Usable GPIO for the player: 1–18, 21, 38–48
(minus strapping care on 0, 3, 45, 46), with 19/20 reserved for USB.

---

## 2. DAC / headphone driver — Cirrus Logic CS43131

- Product page: https://www.cirrus.com/products/cs43131/
- Datasheet (DS1155F2):
  https://statics.cirrus.com/pubs/proDatasheet/CS43131_DS1155F2.pdf
- DigiKey CS43131-CWZR (42-ball WLCSP, Rev A):
  https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CWZR/7430364
- DigiKey CS43131-CNZ (40-pin QFN, Stage 2 carrier):
  https://www.digikey.com/en/products/detail/cirrus-logic-inc/CS43131-CNZ/7388594
- Evaluation kit manual (reference schematic/layout):
  https://statics.cirrus.com/pubs/rdDatasheet/CDB43131-GBK_DS1155V2DB1.pdf

| Name | QFN-40 pin | WLCSP ball | Domain | I/O | Function |
|---|---|---|---|---|---|
| SCL | 1 | B5 | VL | I | I²C clock |
| SDIN1 | 2 | A6 | VL | I | Serial audio data in 1 |
| TSO | 3 | B6 | — | I/O | Test output |
| VD | 4 | C6 | — | P | Internal digital supply, 1.8 V |
| FILT+ | 5 | D4 | VA | O | DAC positive reference (capacitor) |
| FILT– | 6 | D5 | VA | O | DAC negative reference (capacitor) |
| VA | 7 | D6 | — | P | Analog supply, 1.8 V |
| GNDA | 8 | E5 | — | G | Analog ground |
| –VA | 9 | E6 | VA | O | Negative charge pump output for DAC rail |
| FLYP_VA | 10 | F6 | VA | O | –VA charge pump flying cap + |
| FLYN_VA | 11 | G6 | VA | O | –VA charge pump flying cap − |
| HPINA | 12 | G5 | VCP_FILT± | I | Analog bypass input A |
| HPREFA | 13 | E4 | VCP_FILT± | I | Headphone reference A |
| HPOUTA | 14 | G4 | VCP_FILT± | O | Headphone output A |
| HPINB | 15 | F3 | VCP_FILT± | I | Analog bypass input B |
| HPOUTB | 16 | G3 | VCP_FILT± | O | Headphone output B |
| HPREFB | 17 | E3 | VCP_FILT± | I | Headphone reference B |
| VCP_FILT– | 18 | G2 | VCP/VP | I/O | Inverting charge pump filter − |
| GNDCP | 19 | F2 | — | G | Charge pump ground |
| FLYN_VCP | 20 | G1 | VCP_FILT± | O | –VCP charge pump flying cap − node |
| VCP_FILT+ | 21 | E2 | VCP/VP | I/O | Inverting charge pump filter + |
| HP_DETECT | 22 | F4 | VP | I | Headphone detect (debounced) |
| FLYC_VCP | 23 | F1 | VCP/VP | O | –VCP flying cap center node |
| FLYP_VCP | 24 | E1 | VCP/VP | O | –VCP flying cap + node |
| VCP | 25 | D2 | — | P | Charge pump supply, 1.8 V |
| VP | 26 | D1 | — | P | Battery supply, 3.0–5.25 V |
| INT | 27 | C5 | VP | O | Interrupt, open-drain, active low |
| RESET | 28 | C4 | VP | I | System reset |
| DSDB/LRCK2 | 29 | C1 | VL | I/O | DSD data B / LR clock 2 |
| ADR | 30 | C2 | VL | I | I²C address bits (see below) |
| VL | 31 | A1 | — | P | Logic I/O supply, 1.8 V |
| DSDCLK/SCLK2 | 32 | B1 | VL | I/O | DSD clock / bit clock 2 |
| CLKOUT | 33 | B2 | VL | O | Clock output (PLL or buffered crystal) |
| SCLK1 | 34 | A2 | VL | I/O | Serial audio bit clock 1 |
| GNDD | 35 | C3 | — | G | Digital/I/O ground |
| XTO | 36 | A3 | VL | O | Crystal output |
| XTI/MCLK | 37 | A4 | VL | I | Crystal / MCLK input |
| LRCK1 | 38 | B3 | VL | I/O | Serial audio LR clock 1 |
| SDA | 39 | B4 | VL | I/O | I²C data, open-drain |
| DSDA/SDIN2 | 40 | A5 | VL | I | DSD data A / serial data in 2 |
| TSI | — | D3, F5 | — | — | Test input (WLCSP only) |

GNDA, GNDCP, and GNDD must join in a common ground area under the chip.

Recommended operating conditions (Table 3-2):

| Supply | Range |
|---|---|
| VA, VCP, VL, VD | 1.66–1.94 V |
| VP (battery) | 3.0–5.25 V (3.3 V min if HV_EN = 1 or external VCP_FILT) |
| Ambient | −20 to +70 °C |

I²C address low bits from ADR (latched at power-up/reset; Table 4-12): ADR
to GND = `00` (default), 4.99 kΩ to GND = `01`, 4.99 kΩ to VL = `10`,
direct to VL = `11`. The upper address bits are given in the datasheet's
I²C timing figure (Fig. 4-32).

**TinyDAP:** level-shift SCLK1, LRCK1, SDIN1, XTI/MCLK, SDA, SCL (3.3 V ↔
1.8 V). RESET and INT can sit on 3.3 V. VP can come from the battery or
SYS rail. Needs low-noise 1.8 V for VA/VCP and 1.8 V for VL/VD.

---

## 3. Display — Winstar WEO012864D (0.96" 128 × 64, SSD1306)

- Product page:
  https://www.winstar.com.tw/products/oled-module/graphic-oled-display/0_96-oled.html
- DigiKey `WEO012864DWPP3N00006` (hot-bar FPC):
  https://www.digikey.com/en/products/detail/winstar-display/WEO012864DWPP3N00006/20533252
  — spec: https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/5485/WEO012864DWPP3N00006.pdf
- DigiKey `WEO012864DWPP3N00F00` (ZIF FPC):
  https://www.digikey.com/en/products/detail/winstar-display/WEO012864DWPP3N00F00/20533249
  — spec: https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/5485/WEO012864DWPP3N00F00.pdf
- Panel: 26.70 × 19.26 × 1.26 mm, active area 21.738 × 10.858 mm, white,
  SSD1306, 30-pin FPC

| Pin | Symbol | Function | TinyDAP (4-wire SPI) |
|---|---|---|---|
| 1 | NC (GND) | Support pin | GND |
| 2 | C2N | Charge-pump flying cap 2 − | 2.2 µF to C2P |
| 3 | C2P | Charge-pump flying cap 2 + | |
| 4 | C1P | Charge-pump flying cap 1 + | 2.2 µF to C1N |
| 5 | C1N | Charge-pump flying cap 1 − | |
| 6 | VBAT | Charge-pump supply, 3.3–4.2 V | 3.3 V rail |
| 7 | NC | | NC |
| 8 | VSS | Logic ground | GND |
| 9 | VDD | Logic supply, 2.8–3.3 V | 3.3 V |
| 10 | BS0 | Interface select | Low |
| 11 | BS1 | Interface select | Low |
| 12 | BS2 | Interface select | Low |
| 13 | CS# | Chip select (active low) | ESP32 GPIO |
| 14 | RES# | Reset (active low) | ESP32 GPIO |
| 15 | D/C# | Data/command | ESP32 GPIO |
| 16 | R/W# | 6800 R/W or 8080 WR | GND in serial mode |
| 17 | E/RD# | 6800 E or 8080 RD | GND in serial mode |
| 18 | D0 | Data 0 / SCLK in serial mode | SPI SCLK |
| 19 | D1 | Data 1 / SDIN in serial mode | SPI MOSI |
| 20 | D2 | Data 2 | NC in SPI mode |
| 21–25 | D3–D7 | Data 3–7 | GND in serial mode |
| 26 | IREF | Segment current reference | Resistor to VSS (150 kΩ at VCC = 7.5 V internal) |
| 27 | VCOMH | COM high level | Capacitor to VSS |
| 28 | VCC | Panel supply (7.5 V from internal pump) | Capacitor to VSS |
| 29 | VLSS | Analog ground | GND |
| 30 | NC (GND) | Support pin | GND |

Supplies: VDD 2.8–3.3 V; VBAT 3.3–4.2 V for the built-in charge pump
(output 7.0–7.8 V); external VCC 11.5–12.5 V if the pump is unused.
Built-in DC-DC recommended parts: C1, C2 = 2.2 µF; C3, C4 = 1.0 µF;
C5, C6 = 1.0 µF/10 V; R1 (IREF) = 150 kΩ.

BS[2:0] = 000 selects 4-wire SPI, and the D2/D3–D7/R/W#/E/RD# handling in
serial mode follows the SSD1306 datasheet conventions; the Winstar spec
shows the BS table as an image, so confirm against it when drawing the
schematic.

---

## 4. microSD socket — GCT MEM2075-00-140-01-A

- Drawing: https://gct.co/files/drawings/mem2075.pdf
- DigiKey: https://www.digikey.com/en/products/detail/gct/MEM2075-00-140-01-A/9859614
- Type: SMT, push-push, with normally open card-detect switch; 0.5 A per pin

| Pin | Signal | SD bus use |
|---|---|---|
| P1 | DAT2 | 4-bit data 2 |
| P2 | CD/DAT3 | 4-bit data 3 (SPI CS) |
| P3 | CMD | Command (SPI MOSI) |
| P4 | VDD | 3.3 V |
| P5 | CLK | Clock |
| P6 | VSS | Ground |
| P7 | DAT0 | 4-bit data 0 (SPI MISO) |
| P8 | DAT1 | 4-bit data 1 |
| CD (2 terminals) | Card-detect switch | Closes when a card is inserted |
| Shell tabs | Ground | |

**TinyDAP:** 4-bit SDMMC needs CLK, CMD, DAT0–3 (6 GPIO) plus card detect
(1 GPIO); add pull-ups on CMD and DAT0–3 per the SD spec.

---

## 5. USB-C — GCT USB4105-GF-A-060

- Drawing: https://gct.co/files/drawings/usb4105.pdf
- DigiKey: https://www.digikey.com/en/products/detail/gct/USB4105-GF-A-060/14559043
- Type: USB 2.0 Type-C receptacle, 16 pins, SMT; VBUS 5 A combined,
  48 V DC rating; `060` = 0.60 mm shell stake length, `A` = tape and reel

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| A1 | GND | B12 | GND |
| A4 | VBUS | B9 | VBUS |
| A5 | CC1 | B8 | SBU2 |
| A6 | D+ (Dp1) | B7 | D− (Dn2) |
| A7 | D− (Dn1) | B6 | D+ (Dp2) |
| A8 | SBU1 | B5 | CC2 |
| A9 | VBUS | B4 | VBUS |
| A12 | GND | B1 | GND |
| Shell | GND | | |

**TinyDAP:** tie A6/B6 to D+ (ESP32 GPIO20) and A7/B7 to D− (GPIO19).
As a power sink, put a separate 5.1 kΩ pull-down on each of CC1 and CC2.
SBU1/2 unused. ESD protection on VBUS, D+, D− (§21 "ESD: TBD").

---

## 6. Charger — TI BQ25185DLHR

- Product page: https://www.ti.com/product/BQ25185
- Datasheet (SLUSF65B): https://www.ti.com/lit/ds/symlink/bq25185.pdf
- DigiKey: https://www.digikey.com/en/products/detail/texas-instruments/BQ25185DLHR/21769368
- Type: 1-cell linear charger with power path; 3.0–18 V input (25 V
  tolerant); charge current 5 mA–1 A via ISET; input limit 100/500/1100 mA
  via ILIM/VSET
- Package: DLH, 10-pin WSON with thermal pad

| Pin | Name | Type | Function |
|---|---|---|---|
| 1 | SYS | P | Regulated system output, ≥10 µF to GND |
| 2 | BAT | P | Battery +, ≥1 µF to GND |
| 3 | STAT2 | O | Open-drain status, 1–20 kΩ pull-up (≤5 V) |
| 4 | /CE | I | Charge enable, active low |
| 5 | GND | — | Ground |
| 6 | TS/MR | I/O | NTC thermistor input / manual reset |
| 7 | ILIM/VSET | I/O | Input current limit and battery regulation voltage |
| 8 | ISET | I/O | Fast-charge current program (resistor) |
| 9 | STAT1 | O | Open-drain status, 1–20 kΩ pull-up (≤5 V) |
| 10 | IN | P | DC input (USB VBUS), ≥1 µF to GND |
| Pad | Thermal | — | Ground |

**TinyDAP:** IN from USB-C VBUS; SYS feeds the TPS63031 and CS43131 VP;
STAT1/STAT2 can go to ESP32 GPIO with 3.3 V pull-ups for charge status.

---

## 7. 3.3 V regulator — TI TPS63031DSKR + inductor

- Product page: https://www.ti.com/product/TPS63031
- Datasheet (SLVS696D, TPS6303x family): https://www.ti.com/lit/ds/symlink/tps63031.pdf
- DigiKey: https://www.digikey.com/en/products/detail/texas-instruments/TPS63031DSKR/2048021
- Type: buck-boost, fixed 3.3 V output (TPS63031)
- Package: DSK, 10-pin VSON with thermal pad

| Pin | Name | I/O | Function |
|---|---|---|---|
| 1 | VOUT | OUT | Buck-boost output |
| 2 | L2 | IN | Inductor connection |
| 3 | PGND | — | Power ground |
| 4 | L1 | IN | Inductor connection |
| 5 | VIN | IN | Power stage supply |
| 6 | EN | IN | Enable (1 = on) |
| 7 | PS/SYNC | IN | 0 = power-save on, 1 = off, or sync clock |
| 8 | VINA | IN | Control stage supply |
| 9 | GND | — | Control/logic ground |
| 10 | FB | IN | Fixed version: connect to VOUT |
| Pad | Thermal | — | Connected to PGND |

TI reference components (Table 2): L1 1.5 µH; C1 (input) 10 µF 6.3 V 0603
X7R; C2 (output) 2 × 10 µF 6.3 V 0603 X7R; C3 0.1 µF (VINA filter).

**Inductor — Coilcraft LPS3015-152MRC** (TI's reference part: LPS3015,
1.5 µH)
- DigiKey: https://www.digikey.com/en/products/detail/coilcraft/LPS3015-152MRC/12714747
- 1.5 µH ±20%, Isat 2.3 A, 1.7 A rated, 100 mΩ max DCR, shielded,
  2.95 × 2.95 × 1.5 mm, 2 terminals (non-polarized)
- TI-listed alternatives: Coilcraft EPL3010, Murata LQH3NP, Taiyo Yuden
  NR3015 series

**TinyDAP:** PS/SYNC low enables power-save mode for battery life, but
switching-frequency changes can be audible in sensitive audio; Stage 3
should compare both modes (write-up §14, "switching-regulator noise").

---

## 8. Headphone jack — Same Sky SJ-43516-SMT-TR

- Datasheet (SJ-4351X-SMT series):
  https://www.cuidevices.com/product/resource/sj-4351x-smt.pdf
- DigiKey: https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/SJ-43516-SMT-TR/669721
- Type: 3.5 mm, 4-conductor (TRRS), right angle, SMT, tip and ring
  switches; 12 V DC / 1 A; −25 to 85 °C

| Pin | Contact |
|---|---|
| 1 | Sleeve |
| 2 | Tip |
| 3 | Ring 1 |
| 4 | Ring 2 |
| 5 | Tip switch |
| 6 | Ring switch |

**TinyDAP (stereo TRS headphones):** tip = left (HPOUTA), ring 1 = right
(HPOUTB), sleeve and ring 2 together = headphone return (HPREF/ground per
the Cirrus reference design). Tip switch → CS43131 HP_DETECT.

---

## 9. Buttons — GCT SWT0005-015516SSA (×5, plus optional power/hold)

- Drawing: https://gct.co/files/drawings/swt0005.pdf
- DigiKey: https://www.digikey.com/en/products/detail/gct/SWT0005-015516SSA/28022338
- Type: side-actuated tactile, SPST-NO, 3.95 × 1.95 mm, 1.55 mm profile,
  160 gf, 0.15 mm travel, 12 V / 20 mA, 100,000 cycles
- Part code: `0155` = 1.55 mm profile, `16` = 160 gf, `S` = silver, `S` =
  SMT, `A` = tape and reel

| Terminal | Function |
|---|---|
| 1 | Switch contact |
| 2 | Switch contact |

Also has mechanical hold-down pads; use GCT's recommended PCB layout from
the drawing.

**TinyDAP:** each button to a GPIO with internal or external pull-up,
switch to GND.

---

## 10. Battery — 400–600 mAh 1S LiPo (not yet selected)

No specific cell is chosen yet (write-up §30, "Exact LiPo
dimensions/capacity"). Requirements from the spec:

- Single-cell LiPo pouch, 400–600 mAh, protected (protection PCM on the
  cell), positioned behind the PCB
- Connections: **BAT+**, **BAT−**, and optionally an **NTC** thermistor
  lead to the BQ25185 TS/MR pin
- Connector (e.g. JST SH 1.0 mm or PH 2.0 mm, 2 or 3 pin) to be chosen with
  the cell; check the cell's polarity, which is not standardized across
  vendors

---

## 11. Passives — 0201 / 0402 / 0603

No pinout (two terminals). Sizing guidance from the parts above:

- 0603 for bulk/power capacitors where the datasheets call for it (e.g.
  TPS63031 10 µF 6.3 V X7R)
- 0402 for general decoupling and pull-ups
- 0201 only where board area forces it
- Follow the CS43131 reference design (CDB43131 kit manual) for every
  capacitor value, dielectric, and voltage rating around the DAC and its
  charge pumps
