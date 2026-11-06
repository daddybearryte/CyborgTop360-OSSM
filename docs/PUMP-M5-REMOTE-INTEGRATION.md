# M5 Remote integration — pump axis (OSSM-M5-Remote repo)

Same split as rotation's M5 doc: get `struct_message` and the send-side
plumbing in place and bench-testable, hold off on the actual LVGL screen
until you've confirmed the mainboard side spins the motor correctly.
Anchors below are relative to rotation's already-applied edits (e.g.
"alongside `ROT_DIR`"), verified against the current upstream
`ortlof/OSSM-M5-Remote` (`main.cpp` SHA `45f253a...`) to confirm the base
struct/command layout rotation's docs described still matches.

## 1. `src/main.cpp` — mirror `struct_message` — one field, same position as the mainboard edit

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
  float esp_rotation_speed;
  float esp_rotation_pattern;
  float esp_rotation_direction;
  float esp_pump_speed;   // <-- add, same position as mainboard
} struct_message;
```

## 2. Command IDs — alongside `ROT_DIR`

Must match the mainboard's values exactly, same as rotation's:

```cpp
#define ROT_PATTERN 33
#define ROT_DIR 34
#define PUMP_ON 35        // <-- add
#define PUMP_OFF 36       // <-- add
#define PUMP_SPEED 37     // <-- add

#define CONNECT 88
#define HEARTBEAT 99
```

## 3. Sending commands — mirrors `rotationStart()`/`rotationStop()`/`rotationSetSpeed()`

```cpp
void pumpStart() {
  SendCommand(PUMP_ON, 0.0, OSSM_ID);
}

void pumpStop() {
  SendCommand(PUMP_OFF, 0.0, OSSM_ID);
}

// value: 0-100, same convention as speed/rotation
void pumpSetSpeed(float percent) {
  SendCommand(PUMP_SPEED, percent, OSSM_ID);
}
```

No pattern-select equivalent — pump has nothing analogous to
`rotationSetPattern()`.

## 4. Receiving pump state — same spot rotation's fields were added

```cpp
    pattern = incomingcontrol.esp_pattern;
    LogDebug(pattern);
    float rotationSpeed = incomingcontrol.esp_rotation_speed;
    int rotationPattern = (int)incomingcontrol.esp_rotation_pattern;
    float pumpSpeed = incomingcontrol.esp_pump_speed;   // <-- add
```

Same reasoning as rotation: no display widget exists yet, this just keeps
a module-scope variable current so the screen work later wires a label to
something already live.

## What's still open

- **The actual screen** — title + speed slider + Start/Stop, no pattern
  roller needed (pump has none). Do this once the mainboard's `ledcWrite`
  is confirmed actually driving the MOSFET/pump correctly on the bench —
  same "don't design a screen for an axis you haven't seen work yet"
  discipline as rotation.
- **Menu placement** — same open question rotation's doc left: own
  top-level screen (`ui_Pump`, reached from the Menu screen like
  `ui_Rotation` was) is the natural default given that's the pattern
  every other screen in this UI already follows. Worth deciding when the
  screen gets built, not now.

Ready to bench-test the mainboard PWM output whenever the MOSFET's wired
up — say the word once you've got pump speed responding to a test command
and I'll build the actual screen next.
