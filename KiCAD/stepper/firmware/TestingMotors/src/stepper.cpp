// =============================================================================
// stepper.cpp — Implementation of the TB6612FNG stepper driver
// =============================================================================

#include "stepper.h"
#include "pins.h"

// ---- Internal state ----
// Tracks which of the 4 full-step phases we are on (0–3).
static uint8_t current_phase = 0;

// =============================================================================
// set_coils() — Apply one of the four full-step phase patterns
// =============================================================================
// Each phase energises both coils in a specific polarity combination.
// The TB6612FNG truth table for one channel:
//   IN1=H, IN2=L  →  motor terminal gets +V  ("forward")
//   IN1=L, IN2=H  →  motor terminal gets -V  ("reverse")
//   IN1=L, IN2=L  →  coast (we use this in stepper_release)
// =============================================================================
static void set_coils(uint8_t phase) {
    switch (phase) {

        case 0:  // A+  B+
            digitalWrite(PIN_AIN1, HIGH);
            digitalWrite(PIN_AIN2, LOW);
            digitalWrite(PIN_BIN1, HIGH);
            digitalWrite(PIN_BIN2, LOW);
            break;

        case 1:  // A-  B+
            digitalWrite(PIN_AIN1, LOW);
            digitalWrite(PIN_AIN2, HIGH);
            digitalWrite(PIN_BIN1, HIGH);
            digitalWrite(PIN_BIN2, LOW);
            break;

        case 2:  // A-  B-
            digitalWrite(PIN_AIN1, LOW);
            digitalWrite(PIN_AIN2, HIGH);
            digitalWrite(PIN_BIN1, LOW);
            digitalWrite(PIN_BIN2, HIGH);
            break;

        case 3:  // A+  B-
            digitalWrite(PIN_AIN1, HIGH);
            digitalWrite(PIN_AIN2, LOW);
            digitalWrite(PIN_BIN1, LOW);
            digitalWrite(PIN_BIN2, HIGH);
            break;
    }
}

// =============================================================================
// stepper_init() — Set up all GPIO pins and enable the driver
// =============================================================================
void stepper_init(void) {
    // Configure direction pins as outputs
    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);
    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);

    // Configure PWM pins as outputs
    pinMode(PIN_PWMA, OUTPUT);
    pinMode(PIN_PWMB, OUTPUT);

    // Configure standby pin as output
    pinMode(PIN_STBY, OUTPUT);

    // Drive both PWM pins HIGH so the H-bridges are fully enabled.
    // (For simple on/off stepping we don't need analogue PWM —
    //  just keep the channels enabled at full power.)
    digitalWrite(PIN_PWMA, HIGH);
    digitalWrite(PIN_PWMB, HIGH);

    // Bring the TB6612FNG out of standby so it can drive the motor.
    digitalWrite(PIN_STBY, HIGH);

    // Start at phase 0
    current_phase = 0;
    set_coils(current_phase);
}

// =============================================================================
// stepper_move() — Move the motor a given number of steps
// =============================================================================
//   steps > 0  →  advance phase 0→1→2→3→0…  (clockwise*)
//   steps < 0  →  advance phase 0→3→2→1→0…  (counter-clockwise*)
//
// * Actual rotation direction depends on wiring — swap a coil pair to reverse.
// =============================================================================
void stepper_move(int steps, unsigned long step_delay_ms) {

    // Determine direction: +1 for forward, -1 for reverse
    int direction = (steps > 0) ? 1 : -1;

    // Work with the absolute number of steps
    int total_steps = abs(steps);

    for (int i = 0; i < total_steps; i++) {

        // Advance (or retreat) through the 4-phase cycle.
        // Adding 4 before the modulo keeps the value positive even when
        // direction is -1.
        current_phase = (current_phase + direction + 4) % 4;

        // Apply the new coil pattern
        set_coils(current_phase);

        // Wait before the next step — this controls the motor speed
        delay(step_delay_ms);
    }
}

// =============================================================================
// stepper_release() — De-energise both coils
// =============================================================================
// Sets all direction pins LOW so no current flows through either coil.
// The motor shaft can now rotate freely (no holding torque).
// =============================================================================
void stepper_release(void) {
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, LOW);
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);
}
