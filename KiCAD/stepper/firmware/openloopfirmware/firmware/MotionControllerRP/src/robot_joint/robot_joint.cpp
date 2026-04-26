// --------------------------------------------------------------------------------------
// Project: MicroManipulatorStepper (Open Loop fork)
// License: MIT (see LICENSE file for full description)
//          All text in here must be included in any redistribution.
// --------------------------------------------------------------------------------------

#include "robot_joint.h"
#include "hw_config.h"
#include "utilities/logging.h"

RobotJoint::RobotJoint(TB6612MotorDriver* motor_driver, int pole_pairs) {
  RobotJoint::motor_driver = motor_driver;
  servo_controller = new ServoController(*motor_driver, pole_pairs);
  position = 0.0f;
  velocity = 0.0f;
}

RobotJoint::~RobotJoint() {
  delete servo_controller;
  delete motor_driver;
  servo_controller = nullptr;
  motor_driver = nullptr;
}

void RobotJoint::init(int joint_idx) {
  RobotJoint::joint_idx = joint_idx;
  servo_controller->init(MOTOR_MAX_CURRENT_FACTOR);
  servo_controller->set_motor_enabled(false, false);
}

bool RobotJoint::calibrate(bool /*print_measurements*/) {
  // Open loop: no calibration required. Return success so the G-code path stays
  // consistent with the original firmware.
  is_calibrated = true;
  is_homed = true;
  LOG_INFO("Joint-%i: open-loop mode, calibration skipped.", joint_idx);
  return true;
}

void RobotJoint::update(float dt, float one_over_dt) {
  servo_controller->update(position, dt, one_over_dt);
}

void RobotJoint::update_target(float p, float v) {
  position = p;
  velocity = v;
}
