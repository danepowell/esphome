#include "icnt86.h"
#include "esphome/core/log.h"

namespace esphome::icnt86 {

static const char *const TAG = "icnt86";

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
}

void ICNT86Touchscreen::update_touches() {
  char buf[100] = {0};
  char mask[1] = {0x00};

  this->i2c_read_byte_(0x1001, buf, 1);
  uint8_t touch_count = buf[0];

  if (touch_count == 0x00 || (touch_count > 5 || touch_count < 1)) {  // No new touch
    return;
  } else {
    this->i2c_read_byte_(0x1002, buf, touch_count * 7);
    this->i2c_write_byte_(0x1001, mask, 1);
    ESP_LOGD(TAG, "Touch count: %d", touch_count);

    for (uint8_t i = 0; i < touch_count; i++) {
      uint16_t x = ((uint16_t) buf[2 + 7 * i] << 8) + buf[1 + 7 * i];
      uint16_t y = ((uint16_t) buf[4 + 7 * i] << 8) + buf[3 + 7 * i];
      uint8_t p = buf[5 + 7 * i];
      uint8_t touch_evenid = buf[6 + 7 * i];
      if (!this->touches_.contains(touch_evenid) ||
          (x != this->touches_[touch_evenid].x_prev && y != this->touches_[touch_evenid].y_prev)) {
        this->add_raw_touch_position_(touch_evenid, x, y, p);
      }
    }
  }
}

void ICNT86Touchscreen::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ICNT86Touchscreen::add_raw_touch_position_(uint8_t id, int16_t x_raw, int16_t y_raw, int16_t z_raw) {
  esphome::touchscreen::TouchPoint tp;
  uint16_t x, y;
  if (this->swap_x_y_) {
    std::swap(x_raw, y_raw);
  }
  if (!this->touches_.contains(id)) {
    tp.state = esphome::touchscreen::STATE_PRESSED;
    tp.id = id;
  } else {
    tp = this->touches_[id];
    tp.state = esphome::touchscreen::STATE_UPDATED;
    tp.y_prev = tp.y;
    tp.x_prev = tp.x;
  }
  tp.x_raw = x_raw;
  tp.y_raw = y_raw;
  tp.z_raw = z_raw;
  if (this->x_raw_max_ != this->x_raw_min_ and this->y_raw_max_ != this->y_raw_min_) {
    x = this->normalize_(x_raw, this->x_raw_min_, this->x_raw_max_, this->invert_x_);
    y = this->normalize_(y_raw, this->y_raw_min_, this->y_raw_max_, this->invert_y_);
    tp.x = x;
    tp.y = y;
  } else {
    tp.state |= esphome::touchscreen::STATE_CALIBRATE;
  }
  if (tp.state == esphome::touchscreen::STATE_PRESSED) {
    tp.x_org = tp.x;
    tp.y_org = tp.y;
  }

  this->touches_[id] = tp;

  this->is_touched_ = true;

  if ((tp.x != tp.x_prev) || (tp.y != tp.y_prev)) {
    this->need_update_ = true;
  }
}

void ICNT86Touchscreen::i2c_write_byte_(uint16_t reg, const char *data, uint8_t len) {
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

void ICNT86Touchscreen::i2c_read_byte_(uint16_t reg, char const *data, uint8_t len) {
  this->i2c_write_byte_(reg, nullptr, 0);
  this->read((uint8_t *) data, len);
}

int16_t ICNT86Touchscreen::normalize_(int16_t val, int16_t min_val, int16_t max_val, bool inverted) {
  int16_t ret;

  // only normalize when min and max value are specified
  if (min_val && max_val) {
    if (val <= min_val) {
      ret = 0;
    } else if (val >= max_val) {
      ret = 0xfff;
    } else {
      ret = (int16_t) ((int) 0xfff * (val - min_val) / (max_val - min_val));
    }
  } else {
    ret = val;
  }
  ret = (inverted) ? 0xfff - ret : ret;

  return ret;
}

}  // namespace esphome::icnt86
