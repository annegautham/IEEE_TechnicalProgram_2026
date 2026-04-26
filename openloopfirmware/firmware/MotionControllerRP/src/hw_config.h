#pragma once
#include "utilities/math_constants.h"

// #define DEMO_MODE

//--- MOTORS ------------------------------------------------------------------

// motor pole pair count
//  * 100 for 0.9deg stepper motors
//  * 50  for 1.8deg stepper motors
constexpr float MOTOR1_POLE_PAIRS = 50;
constexpr float MOTOR2_POLE_PAIRS = 50;
constexpr float MOTOR3_POLE_PAIRS = 50;

// max current factor in range [0..1]. Lower values reduce pwm resolution so a
// value above 0.4 is recommended.
constexpr float MOTOR_MAX_CURRENT_FACTOR = 0.6f;

//--- OPEN LOOP LIMITS --------------------------------------------------------

// Hard caps to protect against step loss. Applied at boot and used to clamp
// any values set via M204.
constexpr float OPEN_LOOP_MAX_LINEAR_ACCEL  = 500.0f;   // mm/s^2
constexpr float OPEN_LOOP_MAX_ANGULAR_ACCEL = 50.0f;    // rad/s^2

//--- KINEMATIC ---------------------------------------------------------------

// Kinematic Parameters are defined kinematic_modes/kinematic_model_delta3d.cpp

// NUM_JOINTS and NUM_TOOLS are defined in 'path_segment.h'. Note that changing
// the number of joints requires changing the kinematic model accordingly and
// also requires the initialization of the correct number of 'RobotJoint' objects
// in the Robtos init method.

//--- PINS --------------------------------------------------------------------

#define JOINT_READY_OVERRIDE

// #define SINGLE_AXIS_BOARD
#ifndef SINGLE_AXIS_BOARD
  // Pins for 3Axis Board
  #define PIN_BUILTIN_LED 25

  #define PIN_M1_PWM_A_POS  13
  #define PIN_M1_PWM_A_NEG  12
  #define PIN_M1_PWM_B_POS  14
  #define PIN_M1_PWM_B_NEG  15

  #define PIN_M2_PWM_A_POS  9
  #define PIN_M2_PWM_A_NEG  8
  #define PIN_M2_PWM_B_POS  10
  #define PIN_M2_PWM_B_NEG  11

  #define PIN_M3_PWM_A_POS  5
  #define PIN_M3_PWM_A_NEG  4
  #define PIN_M3_PWM_B_POS  6
  #define PIN_M3_PWM_B_NEG  7

  #define PIN_MOTOR_EN      18
  #define PIN_MOTOR_PWMAB   19

  #define PIN_TOOL1 16
  #define PIN_TOOL2 17

#else
  // Single Axis Board
  #define PIN_BUILTIN_LED 16
  #define PIN_USER_BUTTON 24

  #define PIN_M1_PWM_A_POS  13
  #define PIN_M1_PWM_A_NEG  12
  #define PIN_M1_PWM_B_POS  14
  #define PIN_M1_PWM_B_NEG  15

  #define PIN_M2_PWM_A_POS  9
  #define PIN_M2_PWM_A_NEG  8
  #define PIN_M2_PWM_B_POS  10
  #define PIN_M2_PWM_B_NEG  11

  #define PIN_M3_PWM_A_POS  5
  #define PIN_M3_PWM_A_NEG  4
  #define PIN_M3_PWM_B_POS  6
  #define PIN_M3_PWM_B_NEG  7

  #define PIN_MOTOR_EN      18
  #define PIN_MOTOR_PWMAB   19
#endif
