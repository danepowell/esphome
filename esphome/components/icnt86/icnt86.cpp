#include "icnt86.h"
#include "esphome/core/log.h"

namespace esphome::icnt86 {

static const char *const TAG = "icnt86";

static const uint16_t REG_TOUCH_STATUS = 0x1001;
static const uint16_t REG_TOUCH_DATA = 0x1002;
static const uint8_t MAX_TOUCHES = 5;
static const uint8_t TOUCH_DATA_LEN = 7;

void ICNT86Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up icnt86 Touchscreen...");

  // Register interrupt pin
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();
  this->attach_interrupt_(interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_();
  }

  this->x_raw_max_ = this->display_->get_native_width();
  this->y_raw_max_ = this->display_->get_native_height();

  // Trigger initial read to activate the interrupt
  this->store_.touched = true;
}

void ICNT86Touchscreen::update_touches() {
  char buf[MAX_TOUCHES * TOUCH_DATA_LEN] = {0};
  char mask[1] = {0x00};

  this->icnt_read_(REG_TOUCH_STATUS, buf, 1);
  uint8_t touch_count = (uint8_t) buf[0];

  if (touch_count > 0 && touch_count <= MAX_TOUCHES) {
    this->icnt_read_(REG_TOUCH_DATA, buf, touch_count * TOUCH_DATA_LEN);
    ESP_LOGV(TAG, "Touch count: %d", touch_count);

    for (uint8_t i = 0; i < touch_count; i++) {
      const char *point = buf + i * TOUCH_DATA_LEN;
      // buf is char, which is signed on this platform, so each byte must be cast before combining
      uint16_t x = ((uint16_t) (uint8_t) point[2] << 8) + (uint8_t) point[1];
      uint16_t y = ((uint16_t) (uint8_t) point[4] << 8) + (uint8_t) point[3];
      uint16_t p = (uint8_t) point[5];
      uint8_t id = (uint8_t) point[6];
      this->add_raw_touch_position_(id, x, y, p);
    }
  }

  // The status register must always be cleared, otherwise the controller keeps the interrupt line
  // asserted and no further edges are generated.
  this->icnt_write_(REG_TOUCH_STATUS, mask, 1);
}

void ICNT86Touchscreen::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ICNT86Touchscreen::i2c_write_byte_(uint16_t reg, char const *data, uint8_t len) {
  char wbuf[50] = {static_cast<char>(reg >> 8 & 0xff), static_cast<char>(reg & 0xff)};
  for (uint8_t i = 0; i < len; i++) {
    wbuf[i + 2] = data[i];
  }
  this->write((const uint8_t *) wbuf, len + 2);
}

void ICNT86Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "icnt86 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

void ICNT86Touchscreen::icnt_read_(uint16_t reg, char const *data, uint8_t len) {
  this->i2c_read_byte_(reg, data, len);
}

void ICNT86Touchscreen::icnt_write_(uint16_t reg, char const *data, uint8_t len) {
  this->i2c_write_byte_(reg, data, len);
}
void ICNT86Touchscreen::i2c_read_byte_(uint16_t reg, char const *data, uint8_t len) {
  this->i2c_write_byte_(reg, nullptr, 0);
  this->read((uint8_t *) data, len);
}

}  // namespace esphome::icnt86
