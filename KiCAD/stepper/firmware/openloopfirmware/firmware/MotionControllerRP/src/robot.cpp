// --------------------------------------------------------------------------------------
// Project: MicroManipulatorStepper
// License: MIT (see LICENSE file for full description)
//          All text in here must be included in any redistribution.
// Author:  M. S. (diffraction limited)
// --------------------------------------------------------------------------------------

#include <LittleFS.h>
#include "robot.h"
#include "hw_config.h"
#include "utilities/logging.h"
#include "utilities/utilities.h"
#include "kinematic_models/kinematic_model_delta3d.h"
#include "robot_joint/robot_joint.h"
#include "pico/multicore.h"
#include "version.h"
#include "robot_tool/pwm_tool.h"
#include <algorithm>

constexpr int SPINLOCK_ID_SHARED_DATA = 0;
constexpr int SPINLOCK_ID_JOINTS = 1;

#include <NeoPixelConnect.h>
NeoPixelConnect led(PIN_BUILTIN_LED, 1);

//*** FUNCTION **************************************************************************

bool startswith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), str.begin());
}

//*** CLASS *****************************************************************************

//--- Robot -----------------------------------------------------------------------------

Robot::Robot(float path_segment_time_step) : 
  path_planner(nullptr, path_segment_time_step), 
  motion_controller(&path_planner),
  servo_loop_frequency_counter(10000),
  motion_controller_frequency_counter(1000),
  shared_data(SPINLOCK_ID_SHARED_DATA),
  joints_spin_lock(spin_lock_instance(SPINLOCK_ID_JOINTS))
{
  kinematic_model = new KinematicModel_Delta3D();
  path_planner.set_kinematic_model(kinematic_model);

  for(int i=0; i<NUM_JOINTS; i++)
    joints[i] = nullptr;

  command_parser.set_command_processor(this);

  current_feedrate = LinearAngular(10.0f, 1.0f);
  max_acceleration = LinearAngular(OPEN_LOOP_MAX_LINEAR_ACCEL, OPEN_LOOP_MAX_ANGULAR_ACCEL);
  path_buffering_time_us = 50*1e3;

  for(int i=0; i<NUM_TOOLS; i++)
    robot_tools[i] = nullptr;

  all_joints_ready = true;  // open loop: no homing/calibration gate
  state = ERobotState::IDLE;
}

Robot::~Robot() {
  if(kinematic_model != nullptr)
    delete kinematic_model;

  for(int i=0; i<NUM_JOINTS; i++) {
    if(joints[i] != nullptr)
      delete joints[i];
    joints[i] = nullptr;
  }
}

void Robot::init() {
  // axis 1
  {
    auto* motor_driver = new TB6612MotorDriver(
      PIN_MOTOR_EN, PIN_M1_PWM_A_POS, PIN_M1_PWM_A_NEG, PIN_MOTOR_PWMAB,
      PIN_MOTOR_EN, PIN_M1_PWM_B_POS, PIN_M1_PWM_B_NEG, PIN_MOTOR_PWMAB
    );
    joints[0] = new RobotJoint(motor_driver, MOTOR1_POLE_PAIRS);
  }

  // axis 2
  {
    auto* motor_driver = new TB6612MotorDriver(
      PIN_MOTOR_EN, PIN_M2_PWM_A_POS, PIN_M2_PWM_A_NEG, PIN_MOTOR_PWMAB,
      PIN_MOTOR_EN, PIN_M2_PWM_B_POS, PIN_M2_PWM_B_NEG, PIN_MOTOR_PWMAB
    );
    joints[1] = new RobotJoint(motor_driver, MOTOR2_POLE_PAIRS);
  }

  // axis 3
  {
    auto* motor_driver = new TB6612MotorDriver(
      PIN_MOTOR_EN, PIN_M3_PWM_A_POS, PIN_M3_PWM_A_NEG, PIN_MOTOR_PWMAB,
      PIN_MOTOR_EN, PIN_M3_PWM_B_POS, PIN_M3_PWM_B_NEG, PIN_MOTOR_PWMAB
    );
    joints[2] = new RobotJoint(motor_driver, MOTOR3_POLE_PAIRS);
  }

  // initialize axes
  for(int i=0; i<NUM_JOINTS; i++) {
    joints[i]->init(i);
  }

  // add tools
  robot_tools[0] = new PwmTool();
  ((PwmTool*)robot_tools[0])->init(PIN_TOOL1, 8000, 8);

  robot_tools[1] = new PwmTool();
  ((PwmTool*)robot_tools[1])->init(PIN_TOOL2, 8000, 8);

  // setup timer for updating the motion controller (which evaluates joint space path
  // segments and produces the current target position for the servo loops)
  float motion_controller_update_time_us = 500;
  add_repeating_timer_us(-motion_controller_update_time_us, 
                         Robot::update_motion_controller_isr, 
                         (void*)this, 
                         &motion_controller_update_timer);
}


void Robot::update_command_parser() {
  // process serial input
  if (Serial.available()) {
    char c = Serial.read();
    command_parser.add_input_character(c);
    // Serial.write(c);
  }

  // update command parse which will queue command to the path planner
  command_parser.update();

  // TESTING:
  //sleep_ms(10);
  //float pos_error = joints[1]->servo_controller->get_position_error();
  //LOG_INFO(">pos_error [µrad]: %f\n", pos_error*1e6);
  //Serial.printf(">pos_error [µrad]: %f\n", pos_error*1e6);#
  //LOG_INFO(">pos_x [mm]: %f", joints[1]->position);
  //update_servo_controllers(0.01f);
}

/**
 * Updates the path planner, that chops up kartesian path segments into joint space
 * path segments using the inverse kinematic model. It then enqueues these joint space path
 * segments for the motion controller.
 */
void Robot::update_path_planner() {
  // check if buffering starts
  uint64_t time = time_us_64();
  if(state == ERobotState::IDLE && path_planner.input_queue_size() > 0) {
    state = ERobotState::BUFFERING_PATH;
    path_buffering_start_time = time;
  }

  // check if execution starts
  uint64_t buffering_time = time-path_buffering_start_time;
  if(state == ERobotState::BUFFERING_PATH && buffering_time > path_buffering_time_us) {
    state = ERobotState::EXECUTING_PATH;
  }

  // execute path
  if(state == ERobotState::EXECUTING_PATH) {
    // update planner and generate joint space path segments
    path_planner.process(true);

    if(path_planner.all_finished())
      state = ERobotState::IDLE;
  }
}

/**
 * Updates the motion controller with a timer interrupt in regular intervals (e.g. 2kHz).
 * The function evaluates joint space path segments and produces the current 
 * target position for the servo loops.
 */
bool Robot::update_motion_controller_isr(repeating_timer_t* timer) {
  float joint_positions[NUM_JOINTS];
  float joint_velocities[NUM_JOINTS];
  float tool_outputs[NUM_TOOLS];

  // get robot pointer
  Robot* robot = (Robot*)timer->user_data;

  // get time and delta time
  uint64_t time_us = time_us_64();
  float dt = float(time_us - robot->last_mc_update_time)*1e-6f;
  robot->last_mc_update_time = time_us;

  // get current joint position/velocity and tool outputs
  bool update_ok = robot->motion_controller.update(dt, joint_positions, joint_velocities, tool_outputs);

  // Attempt to acquire spinlock non-blocking and set new target data for the servo loops
  if (update_ok && spin_try_lock_unsafe(robot->shared_data.lock)) {
    for (int i = 0; i < NUM_JOINTS; i++) {
      robot->shared_data.joint_target_positions[i] = joint_positions[i];
      robot->shared_data.joint_target_velocities[i] = joint_velocities[i];
    }
    spin_unlock_unsafe(robot->shared_data.lock);
  }

  // update tool outputs
  if(update_ok) {
    auto& tools = robot->robot_tools;
    for(int i=0; i<tools.size(); i++) {
      auto* tool = tools[i];
      if(tool != nullptr)
        tool->set_value(tool_outputs[i]);
    }
  }

  // update frequency counter
  robot->motion_controller_frequency_counter.update(dt);

  return true; // keep repeating
}

/**
 * update servo loops, this is called from the second cpu core
 */
void Robot::update_servo_controllers(float dt) {
  float one_over_dt = 1.0f/dt;

  // update axis target position and velocity from shared data
  spin_lock_unsafe_blocking(shared_data.lock);
  for(int i=0; i<3; i++) {
    joints[i]->update_target(shared_data.joint_target_positions[i], 
                             shared_data.joint_target_velocities[i]);
  }
  spin_unlock_unsafe(shared_data.lock);

  // update servo loop for each axis
  spin_lock_unsafe_blocking(joints_spin_lock);
  for(int i=0; i<NUM_JOINTS; i++) {
    joints[i]->update(dt, one_over_dt);
  }
  spin_unlock_unsafe(joints_spin_lock);

  // update frequency counter
  servo_loop_frequency_counter.update(dt);
}

void Robot::enable_servo_control(bool enable) {
  // update motor-command output for each axis (open loop: no homed/calibrated gate)
  spin_lock_unsafe_blocking(joints_spin_lock);
  for(int i=0; i<NUM_JOINTS; i++) {
    joints[i]->servo_controller->set_motor_update_enabled(enable);
  }
  spin_unlock_unsafe(joints_spin_lock);
}

void Robot::set_pose(const Pose6DF& pose) {
  // run inverse kinematic and compute joint positions
  float joint_positions[NUM_JOINTS];
  kinematic_model->inverse(pose, joint_positions);

  while(true) {
    // Attempt to acquire spinlock non-blocking and set new target data for the servo loops
    if (spin_try_lock_unsafe(shared_data.lock)) {
      for (int i = 0; i < NUM_JOINTS; i++) {
        shared_data.joint_target_positions[i] = joint_positions[i];
        shared_data.joint_target_velocities[i] = 0.0f;
        // LOG_DEBUG("Joint-%i: set pose -> angle %f", i, joint_positions[i]);
      }
      spin_unlock_unsafe(shared_data.lock);
      break;
    }
  }

  current_pose = pose;
}

Pose6DF Robot::pose_from_joint_angles() {
  // read joint positions from encoders
  float joint_pos[NUM_JOINTS];

  spin_lock_unsafe_blocking(joints_spin_lock);
  for (int i = 0; i < NUM_JOINTS; i++) {
    joint_pos[i] = joints[i]->servo_controller->read_position(); 
  }
  spin_unlock_unsafe(joints_spin_lock);

  // run foreward kinematic model to retrieve pose from joint positions
  Pose6DF pose;
  bool ok = kinematic_model->foreward(joint_pos, pose);
  if(ok == false)
    LOG_ERROR("Foreward kinematic failed");

  return pose;
}

CommandParser* Robot::get_command_parser() {
  return &command_parser;
}

bool Robot::check_all_joints_ready() {
  bool all_ready = true;
  for(int i=0; i<NUM_JOINTS; i++) {
    all_ready &= joints[i]->is_calibrated && joints[i]->is_homed;
  }

  return all_ready;
}

bool Robot::home(uint8_t joint_mask, float /*retract_angles*/[NUM_JOINTS]) {
  // Open loop "homing" = declare current commanded joint position as the origin.
  // No motor motion; matches a set-origin button.
  LOG_INFO("G28: setting current pose as origin (open-loop mode)");

  spin_lock_unsafe_blocking(joints_spin_lock);
  spin_lock_unsafe_blocking(shared_data.lock);
  for(int i=0; i<NUM_JOINTS; i++) {
    if(((joint_mask>>i)&1) == 0) continue;
    joints[i]->servo_controller->set_position(0.0f);
    shared_data.joint_target_positions[i] = 0.0f;
    shared_data.joint_target_velocities[i] = 0.0f;
    joints[i]->is_homed = true;
    joints[i]->is_calibrated = true;
  }
  spin_unlock_unsafe(shared_data.lock);
  spin_unlock_unsafe(joints_spin_lock);

  // Recompute Cartesian pose from the (now-zeroed) joint angles.
  set_pose(pose_from_joint_angles());

  all_joints_ready = check_all_joints_ready();
  return true;
}

bool Robot::calibrate_joint(int joint_idx, bool /*store_calibration*/, bool /*print_measurements*/) {
  if(joint_idx<0 || joint_idx >= NUM_JOINTS)
    return false;

  // Open loop: no calibration routine. Mark as ready and return.
  joints[joint_idx]->is_calibrated = true;
  joints[joint_idx]->is_homed = true;
  all_joints_ready = check_all_joints_ready();
  return true;
}

//--- G-Code Commands -------------------------------------------------------------------

bool Robot::can_process_command(const GCodeCommand& cmd) {
  if(cmd.get_command() == "G0" || 
     cmd.get_command() == "G4")
  {
    return path_planner.input_queue_full() == false;
  }

  return true;
}

void Robot::send_reply(const char* str) {
  Serial.write(str);
}

void Robot::process_command(const GCodeCommand& cmd, std::string& reply) {
  if(cmd.get_command() == "G0") process_motion_command(cmd, reply);
  else if(cmd.get_command() == "G1") process_motion_command(cmd, reply);
  else if(cmd.get_command() == "G4") process_dwell_command(cmd, reply);
  else if(cmd.get_command() == "G24") process_set_pose_command(cmd, reply);
  else if(cmd.get_command() == "G28") process_home_command(cmd, reply);
  else if(startswith(cmd.get_command(), "M")) process_machine_command(cmd, reply);
  else reply="error: unknown command\n";
}

void Robot::process_machine_command(const GCodeCommand& cmd, std::string& reply) {
  reply = "";

  // process tool output command
  if(cmd.get_command() == "M3") {
    process_tool_output_command(cmd, reply);
    return;
  }

  // enable motors
  if(cmd.get_command() == "M17") {
    // Sync pose FIRST — this updates shared_data with the correct joint targets
    // so that when servo control is enabled, core1 drives to the right positions.
    // (Matches original repo order: set_pose before enabling motors.)
    set_pose(pose_from_joint_angles());

    // Enable motor drivers (GPIO18 HIGH, sync field angle, set amplitude)
    spin_lock_unsafe_blocking(joints_spin_lock);
    for(int i=0; i<NUM_JOINTS; i++) {
      joints[i]->servo_controller->set_motor_enabled(true, true);
    }
    spin_unlock_unsafe(joints_spin_lock);

    // Now safe to let core1 drive field angles — shared_data already has correct targets.
    enable_servo_control(true);

    reply = "ok\n";
    return;
  }

  // disable motors
  if(cmd.get_command() == "M18") {
    enable_servo_control(false);
    spin_lock_unsafe_blocking(joints_spin_lock);
    for(int i=0; i<NUM_JOINTS; i++)
      joints[i]->servo_controller->set_motor_enabled(false, false);
    spin_unlock_unsafe(joints_spin_lock);

    reply = "ok\n";
    return;
  }

  // get current internal position (not using encoders to read physical position)
  if(cmd.get_command() == "M50") {
    reply += std::string("X") + std::to_string(current_pose.translation.x);
    reply += std::string(" Y") + std::to_string(current_pose.translation.y);
    reply += std::string(" Z") + std::to_string(current_pose.translation.z);
    reply += "\nok\n";
    return;
  }

  // get current per-joint commanded angle (open loop has no encoders)
  if(cmd.get_command() == "M51") {
    for(int i=0; i<NUM_JOINTS; i++) {
      float angle = joints[i]->servo_controller->get_position() * Constants::RAD2DEG;
      reply += std::string("Joint ")+std::to_string(i)+":  " +
               std::to_string(angle) + " deg  (open-loop)\n";
    }
    reply += "ok\n";
    return;
  }

  // get planner queue size
  if(cmd.get_command() == "M52") {
    int s = path_planner.input_queue_size();
    reply += std::string("Queue Size: ") + std::to_string(s) + "\n";
    reply += "ok\n";
    return;
  }

  // check if all planned motions are finished executing
  if(cmd.get_command() == "M53") {
    bool f = path_planner.all_finished();
    reply += f ? "1\n" : "0\n";
    reply += "ok\n";
    return;
  }

  // set servo loop parameters
  if(cmd.get_command() == "M55") {
    process_set_servo_parameter_command(cmd, reply);
    return;
  }

  // calibrate joint
  if(cmd.get_command() == "M56") {
    process_calibrate_joint_command(cmd, reply);
    return;
  }

  // get info
  if(cmd.get_command() == "M57") {
    uint32_t servo_loop_freq = servo_loop_frequency_counter.get();
    uint32_t mcontroler_freq = motion_controller_frequency_counter.get();

    spin_lock_unsafe_blocking(joints_spin_lock);
    for(int i=0; i<NUM_JOINTS; i++) {
      float angle = joints[i]->servo_controller->get_position()*Constants::RAD2DEG;
      reply += std::string("Joint ") + std::to_string(i)+":";
      reply += std::string("  is_homed=") + std::to_string(joints[i]->is_homed);
      reply += std::string(",  is_calibrated=") + std::to_string(joints[i]->is_calibrated);
      reply += std::string(",  commanded_angle=") + std::to_string(angle) + " deg";
      reply += std::string(",  mode=open-loop\n");
    }
    spin_unlock_unsafe(joints_spin_lock);

    reply += std::string("Control Loop: ") + std::to_string(servo_loop_freq/1000) + " kHz\n";
    reply += std::string("Motion Controler: ") + std::to_string(mcontroler_freq) + " Hz\n";

    for(int i=0; i<NUM_TOOLS; i++)
        reply += std::string("Tool[") + std::to_string(i) + "] output: " + std::to_string(current_tool_outputs[i]) + "\n";

    // file list
    reply += std::string("Files on flash: \n");
    auto file_list = get_file_list("/", true);
    for(auto& f : file_list) reply += std::string("  ")+f+"\n";
    reply += "ok\n";
    return;
  }

  // get firmware version
  if(cmd.get_command() == "M58") {
    reply = std::string(FIRMWARE_VERSION)+"\n";
    reply += "ok\n";
    return;
  }

  // M59 removed: no encoder LUT in open-loop mode

  // M60: direct joint spin — bypasses kinematics, drives one motor shaft directly.
  // Usage: M60 J<0-2> A<delta_deg> S<speed_deg_per_sec>
  // Defaults: J2  A360  S180
  if(cmd.get_command() == "M60") {
    int joint_idx  = (int)cmd.get_value('J', 2.0f);
    float delta_deg = cmd.get_value('A', 360.0f);
    float speed_dps = cmd.get_value('S', 180.0f);

    if(joint_idx < 0 || joint_idx >= NUM_JOINTS) {
      reply = "error: joint index out of range\n";
      return;
    }

    float delta_rad = delta_deg * Constants::DEG2RAD;
    float speed_rps = speed_dps * Constants::DEG2RAD;
    float pole_pairs = joints[joint_idx]->servo_controller->get_pole_pair_count();

    LOG_INFO("M60: J=%d A=%.2f deg S=%.2f dps  (delta_rad=%.4f speed_rps=%.4f pp=%.1f)",
             joint_idx, delta_deg, speed_dps, delta_rad, speed_rps, pole_pairs);

    // Snapshot the starting field BEFORE we change any servo state so we can
    // diagnose whether the field actually moved.
    float start_field = joints[joint_idx]->servo_controller->get_motor_driver().get_field_angle();

    // Pause servo-loop output for this joint so rotate_field owns set_field_angle.
    // Do this BEFORE reading start_field's companion below, because core1 is otherwise
    // continuously re-writing field_angle.
    spin_lock_unsafe_blocking(joints_spin_lock);
    joints[joint_idx]->servo_controller->set_motor_update_enabled(false);
    spin_unlock_unsafe(joints_spin_lock);

    sleep_ms(2); // let any in-flight servo update finish

    // Re-snapshot after disabling (in case core1 wrote one last value).
    start_field = joints[joint_idx]->servo_controller->get_motor_driver().get_field_angle();

    joints[joint_idx]->servo_controller->get_motor_driver().rotate_field(
        delta_rad * pole_pairs,
        speed_rps * pole_pairs,
        nullptr
    );

    float end_field = joints[joint_idx]->servo_controller->get_motor_driver().get_field_angle();
    LOG_INFO("M60: field swept %.4f -> %.4f rad (delta=%.4f, expected=%.4f)",
             start_field, end_field, end_field - start_field, delta_rad * pole_pairs);

    // Align the servo's internal motor_pos AND shared_data target to the new physical
    // rotor position, so that when we re-enable motor updates, core1's computed
    // field angle matches where rotate_field left the field.
    // (set_position updates servo_controller::motor_pos without touching the field.)
    float new_motor_pos = joints[joint_idx]->servo_controller->get_position() + delta_rad;
    joints[joint_idx]->servo_controller->set_position(new_motor_pos);

    spin_lock_unsafe_blocking(shared_data.lock);
    shared_data.joint_target_positions[joint_idx] = new_motor_pos;
    shared_data.joint_target_velocities[joint_idx] = 0.0f;
    spin_unlock_unsafe(shared_data.lock);

    // Re-enable servo output — core1 will now compute field = new_motor_pos * pp,
    // which should equal end_field (continuity).
    spin_lock_unsafe_blocking(joints_spin_lock);
    joints[joint_idx]->servo_controller->set_motor_update_enabled(true);
    spin_unlock_unsafe(joints_spin_lock);

    reply = "ok\n";
    return;
  }

  // set linear and angular acceleration (hard-clamped to open-loop caps)
  if(cmd.get_command() == "M204") {
    if(cmd.has_word('L')) {
      float l = cmd.get_value('L');
      max_acceleration.linear = std::min(l, OPEN_LOOP_MAX_LINEAR_ACCEL);
    }
    if(cmd.has_word('A')) {
      float a = cmd.get_value('A');
      max_acceleration.angular = std::min(a, OPEN_LOOP_MAX_ANGULAR_ACCEL);
    }
    reply += "ok\n";
    return;
  }
}

void Robot::process_motion_command(const GCodeCommand& cmd, std::string& reply) {
  Pose6DF end_pose;
  
  #ifndef JOINT_READY_OVERRIDE
  if(!all_joints_ready) {
    reply = "error: not all joints calibrated and homed\n";
    return;
  }
  #endif
  
  if(path_planner.input_queue_full()) {
    reply = "busy\n";
    return;
  }

  // read feed rate
  current_feedrate.linear = cmd.get_value('F', current_feedrate.linear);
  current_feedrate.angular = cmd.get_value('R', current_feedrate.angular);

  if(cmd.has_word('I'))
    state = ERobotState::EXECUTING_PATH;

  // read translation
  end_pose.translation.x = cmd.get_value('X', current_pose.translation.x);
  end_pose.translation.y = cmd.get_value('Y', current_pose.translation.y);
  end_pose.translation.z = cmd.get_value('Z', current_pose.translation.z);

  // read rotation (all elements must be present)
  if(cmd.has_word('A') && cmd.has_word('B') && cmd.has_word('C')) {
    Vec3F rot_vec(cmd.get_value('A'), cmd.get_value('B'), cmd.get_value('C'));
    end_pose.rotation = QuaternionF::from_rot_vec(rot_vec);
  } else {
    end_pose.rotation = current_pose.rotation;
  }

  // create path segment
  CartesianPathSegment path_segment(current_pose, 
                                    end_pose, 
                                    current_feedrate, 
                                    max_acceleration,
                                    current_tool_outputs);

  bool ok = path_planner.add_cartesian_path_segment(path_segment);
  if(ok) {
    path_planner.run_look_ahead_planning();
    current_pose = end_pose;
    reply = "ok\n";
  } else {
    reply = "error\n";
  }
}

/**
 * Immediately sets the current pose without creating path segments or 
 * interpolating from current position. Useful for external realtime controll.
 */
void Robot::process_set_pose_command(const GCodeCommand& cmd, std::string& reply) {
  Pose6DF pose;

  if(!all_joints_ready) {
    reply = "error: not all joints calibrated and homed\n";
    return;
  }

  // read translation
  pose.translation.x = cmd.get_value('X', current_pose.translation.x);
  pose.translation.y = cmd.get_value('Y', current_pose.translation.y);
  pose.translation.z = cmd.get_value('Z', current_pose.translation.z);

  // read rotation (all elements must be present)
  if(cmd.has_word('A') && cmd.has_word('B') && cmd.has_word('C')) {
    Vec3F rot_vec(cmd.get_value('A'), cmd.get_value('B'), cmd.get_value('C'));
    pose.rotation = QuaternionF::from_rot_vec(rot_vec);
  } else {
    pose.rotation = current_pose.rotation;
  }

  // set the current pose und update target angles for servo loops
  set_pose(pose);
  reply = "ok\n";
}

void Robot::process_dwell_command(const GCodeCommand& cmd, std::string& reply) {
  #ifndef JOINT_READY_OVERRIDE
  if(!all_joints_ready) {
    reply = "error: not all joints calibrated and homed\n";
    return;
  }
  #endif

  if(path_planner.input_queue_full()) {
    reply = "busy\n";
    return;
  }

  // get dwell time
  float dwell_time = 1.0f;
  if(cmd.has_word('S')) dwell_time = cmd.get_value('S');          // time given in seconds
  if(cmd.has_word('P')) dwell_time = cmd.get_value('P')*0.001f;   // time given in milliseconds

  // create path segment
  CartesianPathSegment path_segment(current_pose, current_tool_outputs, dwell_time);
  bool ok = path_planner.add_cartesian_path_segment(path_segment);

  if(ok) {
    path_planner.run_look_ahead_planning();
    reply = "ok\n";
  } else {
    reply = "error\n";
  }
}

void Robot::process_set_servo_parameter_command(const GCodeCommand& /*cmd*/, std::string& reply) {
  // Open loop: no PID controllers to tune. Accept the command so existing
  // client scripts don't break.
  reply = "ok\n";
}

void Robot::process_home_command(const GCodeCommand& cmd, std::string& reply) {
  float retract_angles[NUM_JOINTS] = {-1.0f};

  // TODO: check parameter and build joint mask
  uint8_t joint_mask = 0;
  for(int i=0; i<NUM_JOINTS; i++) {
    char word = 'A'+i;
    if(cmd.has_word(word)) {
      joint_mask |= 1<<i;
      float retract_angle = cmd.get_value(word) * Constants::DEG2RAD;
      if(retract_angle > 1e-3f)
        retract_angles[i] = retract_angle;
    }
  }

  std::string supported_words = "A,B,C,D,E,F";
  if(cmd.contains_unsupported_words(supported_words+",G,M")) {
    reply = "error: Unsupported parameter found. Only [" + supported_words + "] are supported\n";
    return;
  }

  if(joint_mask == 0)
    joint_mask = 255;

  bool ok = home(joint_mask, retract_angles);

  reply = ok ? "ok\n" : "error\n";
}

void Robot::process_calibrate_joint_command(const GCodeCommand& cmd, std::string& reply) {
  int idx = cmd.get_value('J', 0);
  bool store_calibration = cmd.has_word('S');
  bool print_measurements = cmd.has_word('P');

  bool ok = calibrate_joint(idx, store_calibration, print_measurements);
  reply = ok ? "ok\n" : "error\n";
}

void Robot::process_tool_output_command(const GCodeCommand& cmd, std::string& reply) {
  // get tool index
  int tool_index = (int)cmd.get_value('T', 0);
  if(tool_index < 0 || tool_index >= NUM_TOOLS) {
    reply = "error: Tool index out of range\n";
    return;
  }

  // set current tool output value
  float tool_value = cmd.get_value('S', 0.0f);
  current_tool_outputs[tool_index] = tool_value;

  reply = "ok\n";
}

