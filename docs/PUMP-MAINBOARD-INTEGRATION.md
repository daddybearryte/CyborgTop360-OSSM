# Mainboard integration — pump axis (OSSM-Stroke repo)

Builds on top of the rotation edits in `MAINBOARD-ROTATION-INTEGRATION.md` —
anchors below are relative to those (e.g. "after the `ROT_DIR` case"), not
raw upstream line numbers, since your fork will already have rotation's
changes in by the time you get to this. Verified against the current
`ortlof/OSSM-Stroke` upstream (`main.cpp` SHA `c979ce5...`, `OSSM_PinDEF.h`
SHA `5126743...`) to confirm IO22 is genuinely free and LEDC channel 4 is
the only one rotation claims.

Unlike rotation, this doesn't need a dedicated engine class or FreeRTOS
task — no pulse train, no pattern table, no direction to manage. It's a
single `ledcWrite()` call driven straight from the command handler.

## 1. `src/OSSM_PinDEF.h` — one more J9 pin

Right after the rotation block:

```cpp
// --- Rotation axis (J9 header, via ULN2803A level shifter) ---
#define ROTATION_PULSE    2    // J9 pin 1 (IO2)  -> ULN2803 input 1
#define ROTATION_DIR      15   // J9 pin 2 (IO15) -> ULN2803 input 2
#define ROTATION_ENABLE   33   // J9 pin 3 (IO33) -> ULN2803 input 3

// --- Pump axis (J9 header, pin 4 — the spare left over from rotation) ---
#define PUMP_PWM          22   // J9 pin 4 (IO22) -> MOSFET gate (via series resistor)
```

J9 is now fully committed — this is the last pin on that header.

**Gate drive note, not a firmware concern but worth having on the bench
before you power anything up:** put a ~150–220Ω resistor between IO22 and
the MOSFET gate, and a ~10kΩ pulldown from gate to source. The pulldown
matters more than it sounds — ESP32 GPIOs can float or glitch briefly
during boot/reset before `setup()` runs and explicitly writes 0, and a
floating gate on a logic-level MOSFET can turn the pump on transiently.
The pulldown holds it off until firmware takes over.

## 2. `src/OSSM_Config.h` — pump PWM constants

Alongside the rotation constants:

```cpp
// --- Pump axis config ---
#define PUMP_PWM_CHANNEL     5      // rotation claims LEDC channel 4 (RotationEngine.h default) - stay clear of it
#define PUMP_PWM_FREQ         20000  // 20kHz - above the audible range, avoids motor whine
#define PUMP_PWM_RESOLUTION   8      // 0-255 duty - plenty for a speed knob, no need for rotation's 10-bit
```

## 3. `src/main.cpp`

### 3a. Command IDs — right after `ROT_DIR` (or after `ROT_PATTERN` if you didn't add `ROT_DIR`)

```cpp
#define ROT_DIR 34
#define PUMP_ON 35        // <-- add
#define PUMP_OFF 36       // <-- add
#define PUMP_SPEED 37     // <-- add
#define CONNECT 88
```

### 3b. Extend `struct_message` — append after `esp_rotation_direction`

Same byte-identical-on-both-repos rule as every other field here:

```cpp
  float esp_rotation_direction;
  float esp_pump_speed;   // <-- add: pump speed state, 0-100%
} struct_message;
```

### 3c. State variables — alongside `speed`/`depth`/`pattern` (the `//Set Values to 0` block)

```cpp
float pumpSpeedPercent = 0;
bool pumpRunning = false;
```

### 3d. `setup()` — right after `Rotator.setMaxRpm(ROTATION_MAX_RPM);`

```cpp
  Rotator.setMaxRpm(ROTATION_MAX_RPM);
  ledcSetup(PUMP_PWM_CHANNEL, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);  // <-- add
  ledcAttachPin(PUMP_PWM, PUMP_PWM_CHANNEL);                       // <-- add
  ledcWrite(PUMP_PWM_CHANNEL, 0);                                  // <-- add: stay off until commanded
```

### 3e. Handle the new commands in `OnDataRecv`'s switch — right after the `ROT_DIR` case

```cpp
            case ROT_DIR: {
              Rotator.setBaseDirection(incomingcontrol.esp_value != 0.0);
              break;
            }
            case PUMP_ON: {                                    // <-- add this whole case
              LogDebug("PUMP_ON Got");
              pumpRunning = true;
              ledcWrite(PUMP_PWM_CHANNEL, map((int)pumpSpeedPercent, 0, 100, 0, 255));
              break;
            }
            case PUMP_OFF: {                                   // <-- add this whole case
              LogDebug("PUMP_OFF Got");
              pumpRunning = false;
              ledcWrite(PUMP_PWM_CHANNEL, 0);
              break;
            }
            case PUMP_SPEED: {                                 // <-- add this whole case
              pumpSpeedPercent = incomingcontrol.esp_value;
              if (pumpRunning) {
                ledcWrite(PUMP_PWM_CHANNEL, map((int)pumpSpeedPercent, 0, 100, 0, 255));
              }
              break;
            }
```

Dialing the speed slider while the pump is off updates `pumpSpeedPercent`
without writing to the PWM channel — turning it back on resumes at
whatever you last set, same as leaving a stove knob where you left it.

### 3f. Broadcast pump state — same two spots as rotation (`homingNotification()` and the reconnect branch)

```cpp
      outgoingcontrol.esp_rotation_direction = 0;
      outgoingcontrol.esp_pump_speed = 0;   // <-- add
```

That's the whole mainboard side. M5 remote side (struct mirror, command
IDs, `SendCommand()` helpers, then the actual screen once you've bench
confirmed the pump responds) follows the same two-stage split as rotation
— see `PUMP-M5-REMOTE-INTEGRATION.md`.
