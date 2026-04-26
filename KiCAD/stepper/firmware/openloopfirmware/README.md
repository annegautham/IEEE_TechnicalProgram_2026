# Open Loop Stepper Firmware

Open-loop firmware for the IEEE Technical Program manipulator. Drives up to
three NEMA-17 bipolar stepper motors via TB6612FNG H-bridges using sinusoidal
microstepping, controlled over USB CDC with a small G-code-style protocol. A
PySide6 desktop GUI is provided for jogging and running test sequences.

This is a fork of the upstream `MicroManipulatorStepperOpenLoop` project,
trimmed for use on the IEEE board.

```
openloopfirmware/
  firmware/MotionControllerRP/   PlatformIO project for the RP2350 Pico 2
  gui/                            PySide6 GUI + serial API
```

---

## Hardware

| Item            | Notes                                                                  |
| --------------- | ---------------------------------------------------------------------- |
| MCU board       | Raspberry Pi **Pico 2** (RP2350)                                       |
| Motor driver    | **TB6612FNG** H-bridge, one channel per motor (M1, M2, M3)             |
| Motor           | NEMA-17 bipolar, 1.8°/step (50 pole pairs). Tested with iMetrx 42BL382 |
| Motor supply    | ~11 V DC, ≥1 A per motor                                               |
| Logic supply    | USB from host computer                                                 |

### Pin map (default, 3-axis board)

Defined in `firmware/MotionControllerRP/src/hw_config.h`. Each motor uses four
PWM pins forming an H-bridge, plus a shared standby line.

| Signal          | M1 (J0) | M2 (J1) | M3 (J2) |
| --------------- | ------- | ------- | ------- |
| `PWM_A_POS`     | GPIO 13 | GPIO 9  | GPIO 5  |
| `PWM_A_NEG`     | GPIO 12 | GPIO 8  | GPIO 4  |
| `PWM_B_POS`     | GPIO 14 | GPIO 10 | GPIO 6  |
| `PWM_B_NEG`     | GPIO 15 | GPIO 11 | GPIO 7  |

Shared:
- `STBY` (driver enable): GPIO 18
- `PWM A/B` (driver mode): GPIO 19
- Tool outputs: GPIO 16, 17
- Built-in LED: GPIO 25

The pin pairs `(POS, NEG)` for a given coil sit on the same RP2350 PWM slice
(slice 6 → GPIO 12/13, slice 7 → 14/15, etc.), which is required for the
sin/cos commutation to switch synchronously.

---

## Wiring a NEMA-17 to a driver channel

> **Read this before connecting any motor.** Mis-wiring two of the four
> motor wires across the wrong driver channels causes a low-impedance short
> through the H-bridge that can destroy a TB6612 in a fraction of a second.

A 4-wire bipolar stepper has **two coils** (call them A and B). For
sinusoidal commutation to produce a rotating field the driver must drive
each coil end-to-end on its own channel. If even one wire of coil A ends
up on channel B (or vice-versa), the field cannot rotate — the motor will
oscillate in place even though it may *hold* fine when stationary.

### Identifying the coil pairs

Wire colour conventions are inconsistent across motors; **always identify
the pairs with a multimeter**, not by colour or pin order.

1. Disconnect all four wires from any driver.
2. Set DMM to continuity (or low-resistance) mode.
3. Probe one wire against each of the other three. Exactly one will read
   ~2–4 Ω (or beep) — those two form **coil A**. The remaining two are
   **coil B**.

For some motors (e.g. Creality / iMetrx extruder steppers wired in a 6-pin
JST connector) the coil pairs are the *non-adjacent* wires (1↔3, 2↔4) when
read in connector order. The continuity test will reveal this.

### Connecting to the screw block

Once you know the pairs:

| Driver screw | Wire                      |
| ------------ | ------------------------- |
| `A1`         | one wire of coil A        |
| `A2`         | the other wire of coil A  |
| `B1`         | one wire of coil B        |
| `B2`         | the other wire of coil B  |

Order *within* a coil pair only changes spin direction — it cannot damage
anything. Order *between* coils (mixing A and B) is what kills the driver.

After clamping the wires, verify at the screws:

- `A1↔A2` beeps (coil A through the motor)
- `B1↔B2` beeps (coil B)
- `A1↔B1`, `A1↔B2`, `A2↔B1`, `A2↔B2` are all silent / open

If any cross-pair beeps, a stray strand is bridging the screws or the wires
are mis-paired. Stop and re-check — do not power on.

### Power-up order

1. USB to Pico (Pico boots and waits in disabled state).
2. Verify motor wiring against the table above.
3. Apply motor supply (~11 V).
4. Send `M17` to enable, then drive.

---

## Building the firmware

The firmware is a [PlatformIO](https://platformio.org/) project targeting
the Pico 2 (`platform = maxgerhardt/raspberrypi`, `board = rpipico2`).

### From the command line

```bash
cd firmware/MotionControllerRP
pio run                       # build
pio run --target upload       # build + upload (auto-detects port)
```

`platformio.ini` does **not** hard-code an upload port — PlatformIO will
auto-detect the Pico's CDC port. If multiple boards are attached, set
`upload_port = /dev/cu.usbmodemXXXX` in `platformio.ini`.

### From VS Code

Open `firmware/MotionControllerRP/` in VS Code with the PlatformIO
extension, then use the status-bar Build / Upload buttons.

### Editing per-motor settings

`src/hw_config.h` exposes the most useful tweakables:

| Constant                       | Default | Meaning                                        |
| ------------------------------ | ------- | ---------------------------------------------- |
| `MOTOR1/2/3_POLE_PAIRS`        | 50      | 50 for 1.8°/step, 100 for 0.9°/step            |
| `MOTOR_MAX_CURRENT_FACTOR`     | 0.6     | PWM amplitude as a fraction of full scale      |
| `OPEN_LOOP_MAX_LINEAR_ACCEL`   | 500     | mm/s² hard cap on cartesian moves              |
| `OPEN_LOOP_MAX_ANGULAR_ACCEL`  | 50      | rad/s² hard cap on joint moves                 |

Pin assignments are in the same file. A `#define SINGLE_AXIS_BOARD` block
selects the alternate single-axis pinout if you're driving the older
1-motor variant.

---

## Running the GUI

```bash
cd gui
python -m venv .venv
source .venv/bin/activate         # Windows: .venv\Scripts\activate
pip install -r source/requirements.txt
python source/main.py             # auto-detects the Pico's CDC port
```

To force a specific port: `python source/main.py /dev/cu.usbmodem1101`.

The GUI exposes:

- **Enable / Disable Motors** — sends `M17` / `M18`.
- **Cartesian jogs** (X±, Y±, Z±) — go through delta-robot kinematics.
- **Joint jogs / spin tests** — drive a single motor.
- **Pose / target readouts** — current cartesian + joint positions.

Cartesian jogs depend on all three motors being connected and configured.
For testing a single motor in isolation, prefer the joint commands or the
raw `M60` command described below.

---

## Serial protocol (G-code-ish)

Default baud is **115 200**, framing is one command per line, terminated
with `\n`. Every accepted command replies with `ok\n`; errors reply with a
descriptive line.

### Motor enable / disable

| Command | Effect                                            |
| ------- | ------------------------------------------------- |
| `M17`   | Pull `STBY` high, energize coils, hold at target  |
| `M18`   | Drop `STBY`, coils float, motor freely turnable   |

`M17` is required before any motion command. Idle hold current is roughly
0.2 A per phase at the default `MOTOR_MAX_CURRENT_FACTOR`.

### Single-joint slow spin (`M60`)

```
M60 J<joint> A<degrees> S<deg/s>
```

Bypasses the kinematic solver and rotates the field for one joint by a
specific mechanical angle at a specific angular speed. Useful for:

- Checking that a freshly wired motor spins smoothly through a full rev.
- Diagnosing whether a stuck/oscillating motor is electrical or mechanical.

Examples:

```
M60 J1 A720 S20      # spin joint 1 two full revolutions at 20°/s (~36 s)
M60 J0 A-90 S5       # creep joint 0 backwards 90° at 5°/s (very slow)
```

The firmware logs the parsed parameters and resulting field sweep:

```
I)M60: J=1 A=720.00 deg S=20.00 dps  (delta_rad=12.5664 ...)
I)M60: field swept 86.37 -> 714.69 rad (delta=628.32, expected=628.32)
ok
```

If the log shows a correct field sweep but the shaft does not rotate, the
fault is downstream of the firmware (driver, wiring, or motor).

### Cartesian motion (`G0` / `G1`)

```
G0 X<mm> Y<mm> Z<mm>          # rapid move through delta IK
G1 X<mm> Y<mm> Z<mm> F<mm/min>
```

Motion is clamped against the workspace bounds and the linear/angular
accel limits in `hw_config.h`. Rejected moves return an error.

### Position set (`M50`)

```
M50 J<joint> V<radians>
```

Forces the firmware's internal joint target/position to a new value
without commanding motion. Use sparingly — primarily for re-zeroing after
manually rotating an unenergized motor.

### Acceleration override (`M204`)

```
M204 P<linear_accel> T<angular_accel>
```

Set the linear (mm/s²) and angular (rad/s²) accel caps at runtime. Values
are clamped against `OPEN_LOOP_MAX_*` from `hw_config.h`.

---

## Quick test recipe

After wiring a single motor to driver M2:

```
M17                       # enable, expect ~0.2 A holding current
M60 J1 A720 S20           # 2 slow full revs on joint 1
M18                       # release
```

Repeat with `J0` (M1 driver) and `J2` (M3 driver) to verify each channel.
A healthy channel will produce smooth continuous rotation with the firmware
log reporting `delta=628.32, expected=628.32` for two revs.

---

## Troubleshooting

| Symptom                                                 | Likely cause                                                                                                    |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| Motor holds fine but oscillates in place on `M60`       | Coil A and B mixed up at the screws (most common). Re-identify pairs with a meter and re-clamp.                 |
| Holds fine on `M60` log but shaft never moves           | One H-bridge channel cooked from a previous short. Probe `A1–A2` vs `B1–B2` in AC volts during `M60` — a dead   |
|                                                         | channel reads near zero. Fix: replace the TB6612, or move the motor to another driver block (M1/M2/M3).         |
| Enable Motors button doesn't change state               | GUI/firmware toggle desynced — click twice, or unplug/replug the Pico USB to reset the firmware.                |
| GUI starts but nothing responds                         | Another process is holding the serial port (commonly an orphan `screen` session). Kill it: `pkill screen`.      |
| `Auto-detected port: ...` but it's the wrong device     | Pass the port explicitly: `python source/main.py /dev/cu.usbmodemXXXX`.                                         |
| Motor is "Y+/Y- mostly works but glitches twice/rev"    | Cartesian jogs go through delta kinematics with reversal points. Use `M60` to spin one joint cleanly.           |
| Holding current reads 0 A even though Enable went green | Wires popped loose at the screws, or the driver chip is dead. Verify `A1↔A2` and `B1↔B2` continuity at the      |
|                                                         | screw heads. Then confirm `VMOTOR` shows ~11 V at the driver input.                                             |

If the firmware appears hung (no `ok` replies, GUI commands ignored): unplug
the Pico USB cable for a few seconds and replug. The Pico re-enumerates and
re-runs setup. The motor supply does not need to be cycled for this.

---

## Safety notes

- **Always power off the motor supply before changing driver wiring.** USB
  alone keeps the Pico alive and the GPIOs ready, but the H-bridge outputs
  are unpowered without `VMOTOR`, so disconnecting at the screw block while
  the motor supply is off is safe.
- Ramp the motor supply up to 11 V — sudden steps can brown out the driver.
- Don't run more than a few seconds of `M60` against a hard stop; the motor
  will dissipate full coil current as heat.
- If you smell anything or feel unusual heat on the driver chip, disable
  immediately (`M18` or unplug supply) and inspect.

---

## License

Inherits the MIT license from the upstream
[MicroManipulatorStepperOpenLoop](https://github.com/diffraction-limited/MicroManipulatorStepperOpenLoop)
project. See `gui/LICENSE`.
