// =============================================================================
// pins.h — Pin assignments and motor constants
// =============================================================================
//
// WIRING OVERVIEW
// ---------------
// The TB6612FNG has two H-bridge channels (A and B).  A bipolar stepper like
// the NEMA 17 has two coils.  We drive one coil with channel A and the other
// with channel B.
//
//   TB6612FNG        Pico 2 GPIO
//   ---------        -----------
//   AIN1             GP2
//   AIN2             GP3
//   BIN1             GP4
//   BIN2             GP5
//   PWMA             GP6   (PWM speed control for coil A)
//   PWMB             GP7   (PWM speed control for coil B)
//   STBY             GP8   (HIGH = chip active, LOW = standby)
//
//   TB6612FNG        NEMA 17
//   ---------        -------
//   AO1              Coil A wire 1  (often BLACK)
//   AO2              Coil A wire 2  (often GREEN)
//   BO1              Coil B wire 1  (often RED)
//   BO2              Coil B wire 2  (often BLUE)
//
//   TB6612FNG        Power
//   ---------        -----
//   VM               Motor supply (8-12 V typical for NEMA 17)
//   VCC              3.3 V from Pico
//   GND              Common ground (Pico + motor supply)
//
// NOTE: Wire colours vary by manufacturer.  If the motor vibrates but doesn't
//       spin, swap one pair (e.g. swap AO1 and AO2).
// =============================================================================

#ifndef PINS_H
#define PINS_H

// --- Motor-driver control pins (directly wired to Pico 2 GPIOs) ---
#define PIN_AIN1  2   // Direction bit 1 for coil A
#define PIN_AIN2  3   // Direction bit 2 for coil A
#define PIN_BIN1  4   // Direction bit 1 for coil B
#define PIN_BIN2  5   // Direction bit 2 for coil B
#define PIN_PWMA  6   // PWM enable for channel A
#define PIN_PWMB  7   // PWM enable for channel B
#define PIN_STBY  8   // Standby pin (must be HIGH for driver to work)

// --- Stepper motor parameters ---
#define STEPS_PER_REV 200   // NEMA 17 = 1.8° per step → 200 steps / revolution

#endif // PINS_H
