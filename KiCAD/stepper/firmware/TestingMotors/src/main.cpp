// =============================================================================
// main.cpp — Demo: spin a NEMA 17 stepper via TB6612FNG on Raspberry Pi Pico 2
// =============================================================================
//
// What this demo does:
//   1. Spin one full revolution clockwise          (200 steps)
//   2. Pause 1 second
//   3. Spin one full revolution counter-clockwise   (200 steps)
//   4. Pause 1 second
//   5. Release the motor (no holding torque)
//   6. Wait 3 seconds, then repeat
//
// Adjust STEP_DELAY_MS to change speed.  Lower = faster, but going below ~2 ms
// risks missed steps (the motor can't keep up).  5–10 ms is a safe start.
// =============================================================================

#include <Arduino.h>
#include "pins.h"
#include "stepper.h"

// Milliseconds between each step — controls motor speed
#define STEP_DELAY_MS 5

void setup() {
    // Start serial so we can print status messages
    Serial.begin(115200);

    // Small delay to let USB serial connect (helpful when debugging)
    delay(2000);
    Serial.println("=== NEMA 17 + TB6612FNG Stepper Demo ===");

    // Initialise the stepper driver (sets up pins, enables the TB6612FNG)
    stepper_init();
    Serial.println("Stepper driver initialised.");
}

void loop() {
    // --- Clockwise rotation ---
    Serial.println("Rotating 1 revolution clockwise...");
    stepper_move(STEPS_PER_REV, STEP_DELAY_MS);       // +200 steps = CW

    delay(1000);  // Pause between direction changes

    // --- Counter-clockwise rotation ---
    Serial.println("Rotating 1 revolution counter-clockwise...");
    stepper_move(-STEPS_PER_REV, STEP_DELAY_MS);      // -200 steps = CCW

    delay(1000);

    // --- Release the motor so it doesn't overheat while idle ---
    Serial.println("Releasing motor (no holding torque).");
    stepper_release();

    delay(3000);  // Wait before the next cycle
}
