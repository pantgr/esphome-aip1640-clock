#pragma once
// AiP1640 LED driver for ESPHome (raw RAM access).
// The AiP1640 (Wuxi i-core) is an 8 SEG x 16 GRID common-cathode LED driver with a 2-wire serial bus,
// pin- and command-compatible with the TM1640. ESPHome has no native driver for either chip.
// Datasheet facts used (AiP1640, i-core rev 2024-01-C4): bus p12 (START/STOP, DATA changes only while
// CLK is low), commands p10-12, init order p13 (clear all 16 RAM bytes BEFORE display on).
// Bit order (LSB first) from the TM1640 datasheet p4-5; the AiP1640 sheet shows it only in a timing diagram.

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::aip1640 {

class AiP1640 final : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_clk_pin(GPIOPin *pin) { this->clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { this->dio_pin_ = pin; }
  void set_brightness(uint8_t b) { this->brightness_ = b & 0x07; }

  // One RAM byte, fixed-address mode (0x44). addr 0..15 = GRID1..GRID16, bit0..7 = SEG1..SEG8.
  void write(uint8_t addr, uint8_t value);
  // All 16 RAM bytes from the shadow buffer, auto-increment mode (0x40).
  void write_all();
  void clear();
  // 0..7 -> pulse width 1/16, 2/16, 4/16, 10/16 .. 14/16 (p12). Display on.
  void brightness(uint8_t b);
  void display_off();

  // Shadow of the chip RAM: fill it from a lambda, then call write_all().
  uint8_t ram[16]{};

 protected:
  void start_();
  void stop_();
  void send_byte_(uint8_t b);
  void command_(uint8_t cmd);

  GPIOPin *clk_pin_{nullptr};
  GPIOPin *dio_pin_{nullptr};
  uint8_t brightness_{0};
};

}  // namespace esphome::aip1640
