#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::icnt86 {

class ICNT86Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void reset_();
  void i2c_read_byte_(uint16_t reg, char const *data, uint8_t len);
  void icnt_read_(uint16_t reg, char const *data, uint8_t len);
  void icnt_write_(uint16_t reg, char const *data, uint8_t len);
  void i2c_write_byte_(uint16_t reg, char const *data, uint8_t len);
  void reset_touch_sensor_();
  void add_raw_touch_position_(uint8_t id, int16_t x_raw, int16_t y_raw, int16_t z_raw = 0);
  int16_t normalize_(int16_t val, int16_t min_val, int16_t max_val, bool inverted = false);
  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{nullptr};
};

}  // namespace esphome::icnt86
