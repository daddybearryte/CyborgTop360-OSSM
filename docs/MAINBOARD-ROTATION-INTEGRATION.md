# Mainboard integration - OSSM-Stroke repo

All line numbers/context below are from the actual `ortlof/OSSM-Stroke` repo
as of 2026-09-10. Copy `lib/RotationEngine/` (the three files alongside this
doc) into your repo's `lib/` folder first - platformio auto-discovers it,
no `platformio.ini` change needed (same reason `lib/StrokeEngine` and
`lib/OssmUi` need no explicit registration).

## 1. `src/OSSM_PinDEF.h` - add the J9 rotation pins

Add after the existing driver pin block:

```cpp
// Pin that pulses on servo/stepper steps - likely labelled PUL on drivers.
#define SERVO_PULSE       14
// Pin connected to driver/servo step direction - likely labelled DIR on drivers.
// N.b. to iHSV57 users - DIP switch #5 can be flipped to invert motor direction entirely
#define SERVO_DIR         27
// Enable Pin Motor Driver
#define SERVO_ENABLE      26

// --- Rotation axis (J9 header, via ULN2803A level shifter) ---
#define ROTATION_PULSE    2    // J9 pin 1 (IO2)  -> ULN2803 input 1
#define ROTATION_DIR      15   // J9 pin 2 (IO15) -> ULN2803 input 2
#define ROTATION_ENABLE   33   // J9 pin 3 (IO33) -> ULN2803 input 3
// J9 pin 4 (IO22) intentionally spare - unused for now
```

IO2 and IO15 are ESP32 strapping pins - this is exactly what the step-2
bench test in the checkpoint doc exists to clear before you rely on them.

## 2. `src/OSSM_Config.h` - rotation tuning constants

Add near the bottom, after `USER_SPEEDLIMIT`:

```cpp
#define USER_SPEEDLIMIT 900 // Speed in Cycles (in & out) per minute.

// --- Rotation axis config ---
#define ROTATION_PULSES_PER_REV   1000    // MUST match Sanyoudian driver DIP switches
#define ROTATION_MAX_RPM          200.0   // ceiling RPM at 100% speed - tune to taste
#define ROTATION_INVERT_DIRECTION false   // flip if it spins backwards at step 9 bring-up
#define ROTATION_ENABLE_ACTIVE_HIGH true  // flip if enable logic is backwards at step 9
```

`ROTATION_PULSES_PER_REV` has no way to be inferred from software - it's
whatever the Sanyoudian's own DIP switches are physically set to (checkpoint
doc: 400-40000 range). Read the DIP switch positions against the driver's
manual and put the matching number here.

## 3. `src/main.cpp`

### 3a. Include, near the other includes (line ~9)

```cpp
#include "RotaryEncoder.h"
#include "RotationEngine.h"     // <-- add this
#include "OssmUi.h"
```

### 3b. New command IDs, in the existing command-ID block (line ~39-54)

30-33 sit in open space above `OTA` (15) and below `CONNECT` (88). Worth
knowing: 20-23 are *also* free on the mainboard (nothing there defines
them), but the M5-Remote repo already claims 20-23 for an unfinished
"CUM" (edging-timer) feature - `CUMSPEED`/`CUMTIME`/`CUMSIZE`/`CUMACCEL`,
sent under a separate `esp_target` value (`CUM = 2`, `src/config.h`) so
there's no actual dispatch collision, and the only code that sends them is
currently wrapped in a block comment (`src/main.cpp` ~line 1081-1124) so
it doesn't even run. Still, picking 30-33 instead of 20-23 avoids two
unrelated features sharing the same literal number if that CUM code is
ever un-commented:

```cpp
#define TORQE_F 6
#define TORQE_R 7
#define OFF 10
...
#define OTA 15 // Need to Uncomment #define OTA_UPDATE if you want to use this command
#define ROT_ON 30        // <-- add
#define ROT_OFF 31       // <-- add
#define ROT_SPEED 32     // <-- add
#define ROT_PATTERN 33   // <-- add
#define CONNECT 88
#define HEARTBEAT 99
```

### 3c. Extend `struct_message` (line ~89-101)

**This struct is raw-`memcpy`'d over ESP-NOW** (see `OnDataRecv`, line 262:
`memcpy(&incomingcontrol, incomingData, sizeof(incomingcontrol));`) - it is
not a self-describing/versioned format. Add the two new fields **at the
end**, and make the *exact same edit, in the exact same field order*, to
`OSSM-M5-Remote`'s copy of this struct (see the companion doc,
`M5-REMOTE-ROTATION-INTEGRATION.md`). If the two repos' structs drift out
of byte-for-byte alignment, every field after the drift point reads garbage
on the other side - this is the single easiest thing to get wrong here.

```cpp
typedef struct struct_message {
  float esp_speed;
  float esp_depth;
  float esp_stroke;
  float esp_sensation;
  float esp_pattern;
  bool esp_rstate;
  bool esp_connected;
  bool esp_heartbeat;
  int esp_command;
  float esp_value;
  int esp_target;
  float esp_rotation_speed;    // <-- add: rotation speed state, 0-100%
  float esp_rotation_pattern;  // <-- add: rotation pattern index, mirrors esp_pattern
} struct_message;
```

### 3d. Instantiate the engine, near `StrokeEngine Stroker;` (line ~174)

```cpp
StrokeEngine Stroker;
RotationEngine Rotator(ROTATION_PULSE, ROTATION_DIR, ROTATION_ENABLE,
                        ROTATION_INVERT_DIRECTION, ROTATION_ENABLE_ACTIVE_HIGH);
TaskHandle_t rotation_T = nullptr;  // add alongside the other TaskHandle_t declarations (line ~197-199)
```

### 3e. Handle the new commands in `OnDataRecv`'s switch (line ~354-359, right after the existing `PATTERN` case)

```cpp
            case PATTERN: {
              int patter = incomingcontrol.esp_value;
              Stroker.setPattern(patter, true);
              LogDebug(Stroker.getPatternName(patter));
              break;
            }
            case ROT_ON: {                                    // <-- add this whole case
              LogDebug("ROT_ON Got");
              Rotator.startPattern();
              break;
            }
            case ROT_OFF: {                                   // <-- add this whole case
              LogDebug("ROT_OFF Got");
              Rotator.stopPattern();
              break;
            }
            case ROT_SPEED: {                                 // <-- add this whole case
              Rotator.setSpeedPercent(incomingcontrol.esp_value);
              break;
            }
            case ROT_PATTERN: {                                // <-- add this whole case
              int rotPattern = incomingcontrol.esp_value;
              Rotator.setPattern(rotPattern);
              LogDebug(Rotator.getPatternName(rotPattern));
              break;
            }
            case TORQE_F: {
```

`ROT_ON`/`ROT_OFF` are separate from the stroke axis's `ON`/`OFF` (which
call `Stroker.startPattern()`/`stopMotion()`) - the two axes start and stop
independently, matching how the M5 remote UI will eventually have separate
stroke and rotation run controls.

### 3f. Broadcast rotation state alongside stroke state

Two spots currently send `esp_speed`/`esp_depth`/`esp_pattern` back to the
M5 remote as connection-state: `homingNotification()` (line ~225-229) and
the remote-reconnect branch in `OnDataRecv` (line ~412-416). Add the
rotation fields to both, e.g. in `homingNotification()`:

```cpp
      outgoingcontrol.esp_connected = true;
      outgoingcontrol.esp_speed = USER_SPEEDLIMIT;
      outgoingcontrol.esp_depth = strokingMachine.physicalTravel;
      outgoingcontrol.esp_pattern = Stroker.getPattern();
      outgoingcontrol.esp_rotation_speed = 0;              // <-- add
      outgoingcontrol.esp_rotation_pattern = Rotator.getPattern(); // <-- add
      outgoingcontrol.esp_target = M5_ID;
```

(Same two lines, same pattern, in the reconnect branch.)

### 3g. `setup()` - bring the engine up and start its task (after line ~524, right after `Stroker.begin(...)`)

```cpp
  Stroker.begin(&strokingMachine, &servoMotor); // Setup Stroke Engine
  Rotator.begin();                              // <-- add: configures J9 pins + LEDC, stays disabled
  Rotator.setPulsesPerRev(ROTATION_PULSES_PER_REV);  // <-- add
  Rotator.setMaxRpm(ROTATION_MAX_RPM);               // <-- add
  if (hardwareVersion >= 20)
  {
    Stroker.enableAndSensorlessHome(&sensorless, homingNotification, 10);
  }
```

And after the existing task-creation block (after line ~560, following the
`espNowRemoteTask` creation):

```cpp
  xTaskCreatePinnedToCore(rotationEngineTask,    /* Task function. */
                            "rotationEngineTask", /* name of task. */
                            2048,                 /* Stack size of task */
                            &Rotator,             /* parameter of the task - pointer to the engine instance */
                            2,                    /* priority - lower than the remote/stroke tasks, higher than idle */
                            &rotation_T,          /* Task handle */
                            1);                   /* pin task to core 1, opposite the ESP-NOW/cable-remote tasks on core 0 */
  delay(100);
```

Core 1 is where `loop()` and `Stroker`'s internal motion task already run
(core 0 is reserved for the emergency-stop, cable-remote, and ESP-NOW
tasks per the existing `xTaskCreatePinnedToCore(..., 0)` calls) - putting
the rotation tick there keeps it off the core handling wireless comms and
E-stop, at some risk of jitter from whatever else core 1 is doing. If the
sweep/escalation patterns read as jittery on the bench, pin it to core 0
instead and watch for any ESP-NOW packet-loss regression - that's an
empirical call, not a settled one.

## Not covered here

Steps (d) and (e) from the "Next Steps" list - accounting for ULN2803's
inverted enable/dir logic and the M5 remote's LVGL UI additions - are
partially covered: the inversion is handled by the `invertDirection`/
`enableActiveHigh` constructor flags (confirm the actual polarity at
build-step-9 bench test and flip the `OSSM_Config.h` defines if backwards).
The LVGL screen work itself (a rotation speed control + pattern roller,
mirroring the existing stroke UI) isn't sketched yet - see the companion
M5-remote doc for what's wired up now vs. what's left.

## 4. Syncing rotation to stroke phase - the "Screw Sync" pattern

`RotationPattern.h` now includes a fifth pattern, `ScrewSync` ("Screw
Sync" in the table, index 4) - forward rotation while the stroke axis is
extending, reverse while it's retracting, constant speed. It needs one
thing none of the other rotation patterns need: to know which way the
stroke axis is currently headed. StrokeEngine already has the hook for
this - `registerTelemetryCallback()` (line 391 in `StrokeEngine.h`) fires
a callback with the target position of every newly-commanded leg, right
when `_applyMotionProfile()` issues `servo->moveTo(pos)` (`StrokeEngine.cpp`
line ~968-982) - i.e. at the start of each leg, not partway through it.
Comparing that position to the previous call's position tells you the
direction of the leg that was just commanded.

### 4a. Add the callback function, above `setup()` (near `homingNotification`, line ~219)

```cpp
// Feeds RotationEngine's ScrewSync pattern. StrokeEngine only supports one
// telemetry callback total (StrokeEngine.h line 427, a single function
// pointer, not a list) - if you ever need telemetry for something else
// too, fan it out from inside this one function rather than trying to
// register a second callback.
float lastStrokeTelemetryPosition = 0.0;
bool haveLastStrokeTelemetryPosition = false;

void strokeTelemetryCallback(float position, float speed, bool clipping) {
  if (haveLastStrokeTelemetryPosition) {
    Rotator.setSyncedStrokeDirection(position > lastStrokeTelemetryPosition);
  }
  lastStrokeTelemetryPosition = position;
  haveLastStrokeTelemetryPosition = true;
}
```

The first callback after boot/homing has no prior sample to compare
against, so it just records a baseline rather than guessing a direction -
`ScrewSync` will hold whatever direction it last had (forward, by
default) for that one leg, then track correctly from the second leg on.

### 4b. Register it in `setup()`, right after `Rotator.begin()` (from section 3g above)

```cpp
  Rotator.begin();
  Rotator.setPulsesPerRev(ROTATION_PULSES_PER_REV);
  Rotator.setMaxRpm(ROTATION_MAX_RPM);
  Stroker.registerTelemetryCallback(strokeTelemetryCallback);   // <-- add
```

### 4c. Using it

Select pattern index 4 (`ROT_PATTERN`, value `4.0`) the same way as any
other rotation pattern - no other wiring needed, `ScrewSync` picks up
`_strokeExtending` from `RotationEngine::update()` every tick
automatically. It runs at whatever speed the rotation speed knob is set
to; only direction is phase-locked.

**Bench-verify at step 9, same as the DIR/ENABLE polarity**: if it spins
the "wrong" way relative to the thread - unscrewing while extending
instead of screwing in - it's not a wiring problem, it's just that
"forward" doesn't happen to correspond to the direction that screws in
for your particular mechanism. Don't re-wire anything; call
`Rotator.setScrewSyncInverted(true);` once, e.g. right after
`Rotator.setMaxRpm(ROTATION_MAX_RPM);` in `setup()`.

**Matches the stroke pattern, not just any pattern selected on the
mainboard**: `ScrewSync` reads whichever pattern `Stroker` is actually
running - it doesn't know or care what that pattern is. Simple two-phase
patterns (Simple Stroke, Teasing/Pounding, Half'n'Half, Deeper) give clean
long holds in each direction. Vibration patterns (Stroke Nibbler, Jack
Hammer) will make it chatter direction rapidly - worth trying once just to
feel it, but probably not the intent behind "screw in / unscrew out."

## 5. Motor-safety override - don't chase vibration-pattern reversals

Rapid, repeated direction reversals are harder on a stepper motor and its
driver than steady rotation - reversing means decelerating the rotor to a
stop and re-accelerating it the other way, every time, and the closed-loop
driver's coils and the Darlington outputs on the ULN2803 all see that as
repeated current transients. `Stroker`'s vibration-heavy stroke patterns
(Jack Hammer, Stroke Nibbler - `lib/StrokeEngine/src/pattern.h`) command
many micro-legs per second, each one a `ScrewSync` direction flip if left
unchecked. Two independent guards handle this, both already built into
`RotationEngine`/`RotationPattern.h` from section 4 above - this section is
just the mainboard-side wiring to actually engage the first one.

**Guard 1 - pattern-aware (the one that actually matters):** when the
active stroke pattern is flagged vibration-heavy, `ScrewSync` stops
tracking `_strokeExtending` altogether and just holds whatever direction
it was last spinning - still rotating, just not chasing the stroke axis
during that pattern. Needs main.cpp to tell `Rotator` which stroke
patterns count as vibration-heavy, since `RotationEngine` itself has no
knowledge of `Stroker`'s pattern table (same separation of concerns as
`RotationPattern.h`'s "self-contained, no hardware knowledge" design).

**Guard 2 - a hard rate floor (backstop):** `RotationEngine` won't
actually flip the DIR pin more than once per `setMinReversalIntervalMs()`
(default 250ms) no matter what any pattern requests - including a future
custom pattern, or `ScrewSync` with the guard above manually disabled.
This is deliberately not tuned against a Sanyoudian spec (not in hand);
treat 250ms as a conservative starting point and only shorten it after
watching the driver/motor behave at the bench.

### 5a. Classify stroke patterns by name, above `setup()` (near the other new free functions from section 4a)

```cpp
// Stroke patterns whose leg structure reverses direction many times per
// second (see lib/StrokeEngine/src/pattern.h) - unsafe to blindly mirror
// onto the rotation axis's direction. Matched by name rather than index so
// this doesn't silently go stale if patternTable[] is ever reordered.
const char* vibrationHeavyStrokePatterns[] = { "Jack Hammer", "Stroke Nibbler" };
const unsigned int vibrationHeavyStrokePatternCount =
    sizeof(vibrationHeavyStrokePatterns) / sizeof(vibrationHeavyStrokePatterns[0]);

bool isVibrationHeavyStrokePattern(int patternIndex) {
  String name = Stroker.getPatternName(patternIndex);
  for (unsigned int i = 0; i < vibrationHeavyStrokePatternCount; i++) {
    if (name == vibrationHeavyStrokePatterns[i]) return true;
  }
  return false;
}
```

If you write a custom stroke pattern later that also reverses rapidly
(a StrokeNibbler variant, say), add its name to this list - that's the
whole extension point.

### 5b. Wire it into the `PATTERN` case, right where the stroke pattern actually changes (section 3e list, or the pre-existing case around line ~354-359)

```cpp
            case PATTERN: {
              int patter = incomingcontrol.esp_value;
              Stroker.setPattern(patter, true);
              LogDebug(Stroker.getPatternName(patter));
              Rotator.setStrokePatternSafeForSync(!isVibrationHeavyStrokePattern(patter));  // <-- add
              break;
            }
```

This is the only place the stroke pattern changes at runtime, so it's the
only place this needs to be called - `Rotator` holds the flag and
re-applies it every tick from then on (`RotationEngine::update()`,
`RotationEngine.cpp`).

### 5c. Initialize it once in `setup()`, defensively

`Stroker`'s pattern defaults to index 0 (Simple Stroke - safe) at boot and
isn't restored from EEPROM (`readEepromSettings()` only restores
`hardwareVersion`), so this isn't strictly load-bearing today - but it
costs one line and stays correct if that ever changes. Add right after the
`Stroker.registerTelemetryCallback(...)` line from section 4b:

```cpp
  Stroker.registerTelemetryCallback(strokeTelemetryCallback);
  Rotator.setStrokePatternSafeForSync(!isVibrationHeavyStrokePattern(Stroker.getPattern()));  // <-- add
```

### 5d. Tuning, if you want it

Neither guard needs tuning to work correctly - defaults are safe. If you
want to loosen guard 2's floor after bench-testing, or turn guard 1 off
entirely to let `ScrewSync` chase every reversal regardless of stroke
pattern (not recommended without first confirming the motor/driver
tolerates it):

```cpp
  Rotator.setMinReversalIntervalMs(150);        // shorten guard 2's floor
  Rotator.setSyncSafetyOverrideEnabled(false);  // disable guard 1 entirely
```

Neither is called by default - both guards are on with their defaults
unless you explicitly add one of these lines.

## 6. Manual direction control (ROT_DIR)

`RotationEngine`/`RotationPattern.h` now support a user-chosen base
direction - `setBaseDirection(bool reverse)` - honored by Steady, Sweep,
Escalation, and Random Bursts (previously hardcoded forward-only), ignored
by Screw Sync (whose direction is phase-derived, not user-chosen). Wiring
it into the command protocol:

### 6a. New command ID, alongside the other ROT_* defines (section 3b)

```cpp
#define ROT_PATTERN 33
#define ROT_DIR 34        // <-- add
```

### 6b. Extend `struct_message` again (section 3c) - one more field, same rules

Same byte-identical-on-both-repos requirement as before:

```cpp
  float esp_rotation_speed;
  float esp_rotation_pattern;
  float esp_rotation_direction;  // <-- add: 0.0 = forward, non-zero = reverse
} struct_message;
```

### 6c. Handle it in `OnDataRecv` (section 3e), alongside the other ROT_* cases

```cpp
            case ROT_DIR: {
              Rotator.setBaseDirection(incomingcontrol.esp_value != 0.0);
              break;
            }
```

### 6d. Broadcast it back on connect/reconnect (section 3f), alongside the other rotation state fields

```cpp
      outgoingcontrol.esp_rotation_speed = 0;
      outgoingcontrol.esp_rotation_pattern = Rotator.getPattern();
      outgoingcontrol.esp_rotation_direction = 0;   // <-- add
```

That's the full mainboard side. The M5 remote side - the actual screen the
"Reverse" checkbox lives on - is covered in
`M5-REMOTE-ROTATION-SCREEN.md`, which also supersedes the "not built yet"
scope note in the original `M5-REMOTE-ROTATION-INTEGRATION.md`.
