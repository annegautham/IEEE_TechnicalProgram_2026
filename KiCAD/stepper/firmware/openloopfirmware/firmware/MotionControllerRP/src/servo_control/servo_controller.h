// --------------------------------------------------------------------------------------
// Project: MicroManipulatorStepper (Open Loop fork)
// License: MIT (see LICENSE file for full description)
//          All text in here must be included in any redistribution.
// --------------------------------------------------------------------------------------

#pragma once

#include "hardware/TB6612_motor_driver.h"

// Open-loop stepper controller. Tracks commanded position directly — no encoder
// feedback. The name `ServoController` is kept for compatibility with the rest
// of the firmware; there is no closed loop inside.
class ServoController {
  public:
    typedef TB6612MotorDriver MOTOR_DRIVER_TYPE;

  public:
    ServoController(MOTOR_DRIVER_TYPE& motor_driver, int32_t motor_pole_pairs);

    void init(float max_motor_amplitude);

    // Command the motor to a target rotor angle. Called from the fast servo-tick
    // context on core1. In open loop this just sets the field angle; `motor_pos`
    // is assumed to follow the command.
    void update(float target_motor_pos, float dt, float one_over_dt);

    // Returns the current (commanded) motor position.
    float read_position();
    float get_position();

    // Always 0 in open loop — kept for API compatibility.
    float get_position_error();

    // Blocking open-loop ramp to a target angle at the given angular velocity.
    void move_to_open_loop(float delta_motor_pos, float motor_angular_velocity);

    // Force the tracked position to a given value (used by set-origin).
    void set_position(float motor_pos);

    MOTOR_DRIVER_TYPE& get_motor_driver();
    float get_pole_pair_count();

    // enable or disable motor current
    void set_motor_enabled(bool enable, bool synchronize_field_angle);

    // enable or disable motor updates (pause field writes)
    void set_motor_update_enabled(bool enable);

  private:
    float motor_pos_to_field_angle(float motor_pos) const;

  private:
    MOTOR_DRIVER_TYPE& motor_driver;

    float motor_pole_pair_count = 0.0f;
    float motor_current_amplitude = 0.5f;
    float motor_pos = 0.0f;              // current commanded motor position
    bool motor_update_enabled = false;
};
