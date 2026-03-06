// =============================================================================
// stepper.h — Simple stepper-motor driver for the TB6612FNG
// =============================================================================
//
// Drives a bipolar stepper (NEMA 17) through a TB6612FNG in full-step mode.
//
// Full-step sequence (two-phase-on):
//   Step 0:  A+  B+      (both coils energised, one direction each)
//   Step 1:  A-  B+
//   Step 2:  A-  B-
//   Step 3:  A+  B-
//
// "A+" means current flows AIN1→high, AIN2→low.
// "A-" means current flows AIN1→low,  AIN2→high.
// Same logic for B channel.
// =============================================================================

#ifndef STEPPER_H
#define STEPPER_H

#include <Arduino.h>

// ---- Public API ----

// Call once in setup() — configures all pins and brings the driver out of standby.
void stepper_init(void);

// Move the motor a given number of steps.
//   steps > 0  →  clockwise
//   steps < 0  →  counter-clockwise
//   step_delay_ms sets the pause between steps (lower = faster, but too low
//   and the motor will skip steps; 5–10 ms is a safe starting range).
void stepper_move(int steps, unsigned long step_delay_ms);

// De-energise both coils (motor can free-spin).  Saves power and reduces heat.
void stepper_release(void);

#endif // STEPPER_H
