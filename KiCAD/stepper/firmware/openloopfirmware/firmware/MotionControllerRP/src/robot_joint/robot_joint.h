// --------------------------------------------------------------------------------------
// Project: MicroManipulatorStepper (Open Loop fork)
// License: MIT (see LICENSE file for full description)
//          All text in here must be included in any redistribution.
// --------------------------------------------------------------------------------------

#pragma once

#include "utilities/logging.h"
#include "utilities/frequency_counter.h"
#include "hardware/TB6612_motor_driver.h"
#include "servo_control/servo_controller.h"
#include "utilities/math_constants.h"

#include "motion_control/path_planner.h"
#include "motion_control/motion_controller.h"
#include "command_parser/command_parser.h"
#include "robot_joint_interface.h"

class RobotJoint {
  public:
    RobotJoint(TB6612MotorDriver* motor_driver, int pole_pairs);
    ~RobotJoint();

    void init(int joint_idx);
    bool calibrate(bool print_measurements);  // no-op in open loop
    void update(float dt, float one_over_dt);
    void update_target(float p, float v);

  public:
    int joint_idx = 0;
    bool is_homed = true;        // open loop: always considered "homed" once inited
    bool is_calibrated = true;   // open loop: no calibration needed

    float position;
    float velocity;

    ServoController* servo_controller;

  private:
    TB6612MotorDriver* motor_driver;
};
