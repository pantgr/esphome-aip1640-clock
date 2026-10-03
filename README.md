# esphome-aip1640-clock

Give a cheap commercial LED clock a new brain: an **ESP32-S3 running ESPHome** drives the clock's
original **AiP1640** display board, so the clock shows time from NTP (or a GPS NTP server on your LAN),
room humidity/temperature from Home Assistant or ESP-NOW, and dims itself at night.

ESPHome has no driver for the AiP1640 (nor for its twin, the TM1640), so this repo provides one as an
**external component**, written from the chip's datasheet, plus two complete clock configurations that
have been in daily use on two clocks since October 2026.

Idea credit: [Adrian's Digital Basement — «Let's make this cheap Wi-Fi clock awesome»](https://www.youtube.com/watch?v=vgfV_IGmhGA)
([HU-058_ESPHome](https://github.com/misterblack1/HU-058_ESPHome)). Our clocks use a different driver chip
(AiP1640 instead of AiP33628), so this is a separate component — if your clock has an AiP1640 or TM1640,
this one is for you.

## What is in here

| Path | What |
|---|---|
| `components/aip1640/` | ESPHome external component: AiP1640 / TM1640 driver (raw RAM access) |
| `examples/clock-espnow.yaml` | Clock with humidity/temperature received over ESP-NOW from another ESP |
| `examples/clock-homeassistant.yaml` | Same clock with humidity/temperature from Home Assistant sensors |
| `examples/secrets.yaml.example` | The secrets the examples expect |
| `gps-ntp/` | Optional: Raspberry Pi + u-blox NEO-M8N as a stratum-1 NTP server for the clocks |

## Hardware

- **Clock board** with an **AiP1640** (SOP28; datasheet: Wuxi i-core, LCSC C82650). Ours came out of a
  commercial «HH:MM 45%RH DD.MM» LED clock; its original MCU board was unplugged, nothing on the display
  board was modified (the original MCU board can go back any time).
- **ESP32-S3 dev board** — we use the YD-ESP32-S3 (ESP32-S3-DevKitC-1 clone, ESP32-S3-N8R2).
- **74HCT245** level shifter. The AiP1640 runs at 5 V and needs **VIH ≥ 0.7·VDD = 3.5 V** (datasheet), more
  than the ESP's 3.3 V. An HCT part reads 3.3 V as HIGH (VIH 2.0 V) and drives full 5 V out.
- **5 V supply** for clock + ESP. See *Power* below — a weak supply resets the ESP.

### Wiring

```
ESP32-S3 GPIO40 ──> 74HCT245 A1 ── B1 ──> AiP1640 pin 8 (CLK)
ESP32-S3 GPIO21 ──> 74HCT245 A2 ── B2 ──> AiP1640 pin 7 (DATA)
74HCT245 pin 20 (VCC) + pin 1 (DIR) ── clock 5 V      (DIR high = A -> B)
74HCT245 pin 19 (/G)  + pin 10 (GND) ── GND           (always enabled)
unused A3..A8 ── GND
ESP32-S3 5Vin ── clock 5 V,  ESP GND ── clock GND
```

No pull-ups were needed through the 74HCT245 (the datasheet's reference circuit shows 10 k pull-ups,
220 Ω series and 100 pF to GND — add them if your lines are long or noisy).

Note: on our boards CLK and DATA were swapped compared with the first plan — if nothing lights up, swap
the two pins in the yaml before anything else.

## Using the component

```yaml
external_components:
  - source: github://pantgr/esphome-aip1640-clock
    components: [aip1640]

aip1640:
  id: aip
  clk_pin: GPIO40
  dio_pin: GPIO21
  brightness: 0      # 0..7, hardware steps
```

From a lambda:

| Call | Does |
|---|---|
| `id(aip).ram[i] = byte;` then `id(aip).write_all();` | Fill the 16-byte shadow RAM, send it all (auto-increment mode) |
| `id(aip).write(addr, byte)` | One RAM byte (fixed-address mode), logged — handy for mapping |
| `id(aip).brightness(0..7)` | Display on at that step |
| `id(aip).display_off()` / `id(aip).clear()` | Off / all segments off |

RAM: address 00H..0FH = GRID1..GRID16, bit0..bit7 = SEG1..SEG8 (datasheet p10).

Brightness: the 8 hardware steps are 1/16, 2/16, 4/16, 10/16 … 14/16 duty. Above step 3 our eyes saw no
difference, so the examples expose 5 levels mapped to hardware 0, 1, 2, 3, 7.

## Display map (our clock board)

Which bit lights which segment is decided by how the LEDs are routed on the clock board, not by the chip,
so it has to be found by eye. On our «HH:MM 45%RH DD.MM» board:

| RAM | Shows | bit7 |
|---|---|---|
| 00H 01H | hours | 00H: dot top-left · 01H: colon upper |
| 02H 03H | minutes | 02H: colon lower |
| 04H 05H | humidity digits | — |
| 06H | «%RH» glyph (0xFF lights exactly that) | RH |
| 07H 08H | day / temperature digits | 07H: upper dot |
| 09H 0AH | month / tenths + «C» | 09H: lower dot (date separator / decimal point) · 0AH: upper dot (°) |

Digits are standard 7-segment, bit0..bit6 = a..g: `0x3F 0x06 0x5B 0x4F 0x66 0x6D 0x7D 0x07 0x7F 0x6F` = 0..9.
GRIDs 0BH..0FH are not used on this board.

### Map your own board

1. Flash an example, then comment out its `interval:` block (it repaints every 500 ms).
2. Open the ESP's web page; the **poke** number takes `address * 256 + byte`.
3. Poke one bit at a time (`0x0001`, `0x0002` … `0x0080`, then `0x0101` …) and write down what lights.

## The clock face (examples)

- `HH:MM` with blinking colon; `--:--` until the first SNTP sync.
- `DD.MM` alternating every 5 s with the room temperature, e.g. `24.5°C` (`-12°C` below -10).
- Room humidity with the `%RH` glyph; dark when the value is stale.
- Auto brightness from any Home Assistant lux sensor: up at 25 / 60 / 250 / 1000 lx, down at 0.6× of each
  threshold (hysteresis). Switchable from HA (`auto brightness`).
- Onboard WS2812 (GPIO48) as a slow rainbow, restored across reboots — optional.
- ESP-NOW payload (`clock-espnow.yaml`), every 30 s from the sensor ESP:
  `{'H', humidity %, 'T', int16 little-endian temperature in 0.1 °C}` (5 bytes).

## Power

Feed the ESP's **5Vin a solid 5.0 V**. On the YD-ESP32-S3, 5Vin goes through a Schottky diode (see the
board's IN-OUT jumper) into a 1117-class 3.3 V regulator that needs roughly 4.4 V at its input. Our clock
that shared a TV box's USB port with two RTL-SDR sticks kept resetting (the box's kernel log showed the
sticks dropping off the bus at the same time); on its own 5 V supply it has run without a single reset.
The likely mechanism is the 3.3 V rail browning out during Wi-Fi transmit peaks — measured cure, not a
measured cause.

## GPS NTP server (optional)

`gps-ntp/` turns a Raspberry Pi with a NEO-M8N (TIMEPULSE on GPIO18) into a stratum-1 NTP server with
gpsd + chrony, so the clocks keep exact time even with the internet down. `ubx_timing_config.sh` sets the
receiver up for timing (stationary model, SBAS off, Galileo on, UART 115200). Read the comments in both
scripts — the NMEA offset must be calibrated on your own setup.

## Credits

- Idea: Adrian Black, [Adrian's Digital Basement](https://www.youtube.com/@adriansdigitalbasement) —
  [video](https://www.youtube.com/watch?v=vgfV_IGmhGA), [HU-058_ESPHome](https://github.com/misterblack1/HU-058_ESPHome).
- Reference AiP1640 driver whose bus timing we followed:
  [spezifisch/esp32-acqi-clock](https://spezifisch.codeberg.page/posts/2023-06-20/gt-acqi-22wo-digital-alarm-clock-hack/)
  (based on [realthk/esphome_tm1640](https://github.com/realthk/esphome_tm1640) and ESPHome's TM1637 component).
- The driver itself was written from the AiP1640 datasheet (i-core, rev 2024-01-C4) and the TM1640 datasheet.

## License

GPL-3.0 (see `LICENSE`), in line with the ESPHome TM1637 lineage of the reference driver.
