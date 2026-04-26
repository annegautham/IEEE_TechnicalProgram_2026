// --------------------------------------------------------------------------------------
// Project: MicroManipulatorStepper (Open Loop fork)
// License: MIT (see LICENSE file for full description)
//          All text in here must be included in any redistribution.
// --------------------------------------------------------------------------------------

#include "pico/stdlib.h"
#include <cmath>

#include "servo_controller.h"
#include "utilities/logging.h"
#include "utilities/math_constants.h"
#include "hw_config.h"

#include <algorithm>

ServoController::ServoController(MOTOR_DRIVER_TYPE& motor_driver, int32_t pole_pair_count) :
    motor_driver(motor_driver)
{
  motor_pole_pair_count = pole_pair_count;
  motor_update_enabled = false;
  motor_pos = 0.0f;
}

void ServoController::init(float max_motor_amplitude) {
  motor_current_amplitude = max_motor_amplitude;

  motor_driver.begin();              // begin() calls disable() at end → GPIO18 LOW
  motor_driver.set_amplitude(0.0f, true);
  // Do NOT call motor_driver.enable() here.
  // With amplitude=0, enable() would put the TB6612 in SHORT-BRAKE mode (both output
  // pins HIGH), which stiffens the motor shaft even though no current is commanded.
  // enable() is called only inside set_motor_enabled(true, ...).
  motor_driver.set_field_angle(0.0f);
}

void ServoController::update(float target_motor_pos, float /*dt*/, float /*one_over_dt*/) {
  // Open-loop: trust the commanded target. `motor_pos` tracks the command, the
  // field angle is whatever the command says it should be.
  motor_pos = target_motor_pos;

  if (motor_update_enabled) {
    motor_driver.set_field_angle(motor_pos_to_field_angle(motor_pos));
  }
}

float ServoController::read_position() {
  return motor_pos;
}

float ServoController::get_position() {
  return motor_pos;
}

float ServoController::get_position_error() {
  return 0.0f;
}

void ServoController::move_to_open_loop(float delta_motor_pos, float motor_angular_velocity) {
  const bool moving_forward = delta_motor_pos > 0.0f;
  const float abs_delta = fabsf(delta_motor_pos);

  // Snapshot the current field angle — do NOT read motor_pos during the loop
  // because core1 continuously overwrites it with shared_data targets.
  const float start_field = motor_driver.get_field_angle();

  uint64_t last_time = time_us_64();
  float pos = 0.0f;
  while (fabsf(pos) < abs_delta) {
    uint64_t time_us = time_us_64();
    float dt = float(time_us - last_time) * 1e-6f;
    last_time = time_us;

    pos += moving_forward ? motor_angular_velocity * dt : -motor_angular_velocity * dt;

    float clamped = moving_forward ? std::min(pos, abs_delta) : std::max(pos, -abs_delta);
    motor_driver.set_field_angle(start_field + clamped * motor_pole_pair_count);
    sleep_us(100);
  }

  motor_pos += delta_motor_pos;
}

void ServoController::set_position(float new_pos) {
  motor_pos = new_pos;
}

ServoController::MOTOR_DRIVER_TYPE& ServoController::get_motor_driver() {
  return motor_driver;
}

float ServoController::get_pole_pair_count() {
  return motor_pole_pair_count;
}

void ServoController::set_motor_enabled(bool enable, bool synchronize_field_angle) {
  if (enable) {
    motor_driver.enable();  // GPIO18 HIGH (STBY active) — must come before field/amplitude
    if (synchronize_field_angle) {
      motor_driver.set_field_angle(motor_pos_to_field_angle(motor_pos));
    }
    motor_driver.set_amplitude(motor_current_amplitude, true);
  } else {
    motor_driver.set_amplitude(0.0f, true);  // immediately zero amplitude → no coil current
    motor_driver.disable();                  // GPIO18 LOW → STBY off → coils de-energized
  }
}

void ServoController::set_motor_update_enabled(bool enable) {
  motor_update_enabled = enable;
}

float ServoController::motor_pos_to_field_angle(float pos) const {
  // Linear mapping: rotor angle × pole pairs = electrical/field angle.
  // No LUT correction in open loop — step positions are ideal by assumption.
  return pos * motor_pole_pair_count;
}
