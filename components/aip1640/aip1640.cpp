#include "aip1640.h"
#include "esphome/core/log.h"

namespace esphome::aip1640 {

static const char *const TAG = "aip1640";

static constexpr uint8_t CMD_DATA_AUTO = 0x40;   // p10: auto-increment address
static constexpr uint8_t CMD_DATA_FIXED = 0x44;  // p10: fixed address
static constexpr uint8_t CMD_ADDR = 0xC0;        // p11: + 0x0..0xF
static constexpr uint8_t CMD_DISP_OFF = 0x80;    // p12
static constexpr uint8_t CMD_DISP_ON = 0x88;     // p12: + brightness 0..7

// p9: CLK pulse >= 400 ns, setup/hold >= 100 ns. 20 us half-bits are far above that and match the
// working AiP1640 driver in spezifisch/esp32-acqi-clock.
static inline void half_bit() { delayMicroseconds(20); }

void AiP1640::setup() {
  // Idle = both lines high (p12). The datasheet reference circuit (p14) adds 10 k pull-ups; driven
  // through a 74HCT245 they were not needed on our boards.
  this->dio_pin_->setup();
  this->dio_pin_->digital_write(true);
  this->clk_pin_->setup();
  this->clk_pin_->digital_write(true);
  half_bit();

  // p13: power-on RAM is garbage -> clear all 16 bytes, only then display on.
  this->clear();
  this->brightness(this->brightness_);
}

void AiP1640::dump_config() {
  ESP_LOGCONFIG(TAG, "AiP1640:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  DIO Pin: ", this->dio_pin_);
  ESP_LOGCONFIG(TAG, "  Brightness: %u", this->brightness_);
}

void AiP1640::start_() {
  // START = DATA falls while CLK is high (lines idle high).
  this->dio_pin_->digital_write(false);
  half_bit();
}

void AiP1640::stop_() {
  // STOP = DATA rises while CLK is high; DATA may only change while CLK is low.
  this->clk_pin_->digital_write(false);
  half_bit();
  this->dio_pin_->digital_write(false);
  half_bit();
  this->clk_pin_->digital_write(true);
  half_bit();
  this->dio_pin_->digital_write(true);
  half_bit();
}

void AiP1640::send_byte_(uint8_t b) {
  // LSB first (TM1640 sheet p4-5 + reference code; the AiP1640 sheet shows it only in the timing diagram).
  for (uint8_t i = 0; i < 8; i++) {
    this->clk_pin_->digital_write(false);
    half_bit();
    this->dio_pin_->digital_write(b & 0x01);
    half_bit();
    this->clk_pin_->digital_write(true);  // latched on the rising edge
    half_bit();
    this->clk_pin_->digital_write(false);  // back low after every bit, as the reference driver does
    half_bit();
    b >>= 1;
  }
}

void AiP1640::command_(uint8_t cmd) {
  this->start_();
  this->send_byte_(cmd);
  this->stop_();
}

void AiP1640::write(uint8_t addr, uint8_t value) {
  addr &= 0x0F;
  this->ram[addr] = value;
  this->command_(CMD_DATA_FIXED);
  this->start_();
  this->send_byte_(CMD_ADDR | addr);
  this->send_byte_(value);
  this->stop_();
  // p13 fixed-address timing: every write sequence ends with the display control command.
  this->command_(CMD_DISP_ON | this->brightness_);
  ESP_LOGI(TAG, "RAM %02XH = 0x%02X", addr, value);
}

void AiP1640::write_all() {
  this->command_(CMD_DATA_AUTO);
  this->start_();
  this->send_byte_(CMD_ADDR);
  for (uint8_t v : this->ram)
    this->send_byte_(v);
  this->stop_();
  // p12 auto-increment timing: Command1 data, Command2 address + data, Command3 display control.
  this->command_(CMD_DISP_ON | this->brightness_);
}

void AiP1640::clear() {
  for (uint8_t &v : this->ram)
    v = 0;
  this->write_all();
  ESP_LOGI(TAG, "RAM cleared");
}

void AiP1640::brightness(uint8_t b) {
  this->brightness_ = b & 0x07;
  this->command_(CMD_DISP_ON | this->brightness_);
  ESP_LOGI(TAG, "Display on, brightness %u", this->brightness_);
}

void AiP1640::display_off() {
  this->command_(CMD_DISP_OFF);
  ESP_LOGI(TAG, "Display off");
}

}  // namespace esphome::aip1640
