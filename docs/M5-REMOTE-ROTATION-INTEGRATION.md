# M5 Remote integration - OSSM-M5-Remote repo

Scope of this doc: keep `struct_message` byte-identical to the mainboard
and add the plumbing to *send* rotation commands. It does **not** build
the LVGL rotation screen (speed slider + pattern roller mirroring
`ui_PatternS`/`ui_HomePatternLabel`) - that's SquareLine Studio UI design
work, which is its own task ("M5 remote: add rotation control + pattern
selection to LVGL UI" from the checkpoint's Next Steps). What's below gets
you to the point where a placeholder call - a test button, a Serial
command, whatever's convenient on the bench - can drive the rotation axis
end-to-end before the real screen exists.

## 1. `src/main.cpp` - mirror `struct_message` exactly (line ~187-199)

Same struct, same field order, same types as the mainboard edit in
`MAINBOARD-ROTATION-INTEGRATION.md` section 3c. This one **must** match
byte-for-byte - it's `memcpy`'d in both directions over ESP-NOW with no
schema/versioning, so a mismatched struct silently misreads every field
after the mismatch point on whichever side is behind.

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
  float esp_rotation_speed;    // <-- add, same position as mainboard
  float esp_rotation_pattern;  // <-- add, same position as mainboard
} struct_message;
```

## 2. Command IDs (line ~84-104, alongside the existing `#define SPEED 1` block)

Must match the mainboard's values exactly - these are the raw ints
`esp_command` carries over the wire:

```cpp
#define OFF 10
#define ON  11
#define SETUP_D_I 12
#define SETUP_D_I_F 13
#define REBOOT 14
#define ROT_ON 30        // <-- add
#define ROT_OFF 31       // <-- add
#define ROT_SPEED 32     // <-- add
#define ROT_PATTERN 33   // <-- add

#define CONNECT 88
#define HEARTBEAT 99
```

(30-33, not 20-23, to stay clear of the existing-but-commented-out
`CUMSPEED`/`CUMTIME`/`CUMSIZE`/`CUMACCEL` = 20-23 already in this file -
see the mainboard doc for why.)

## 3. Sending commands - `SendCommand(command, value, target)`

The existing helper (line 375) is what every control already uses, e.g.
the pattern roller's `savepattern()` (line 504-510):

```cpp
void savepattern(lv_event_t * e){
  pattern = lv_roller_get_selected(ui_PatternS);
  lv_roller_get_selected_str(ui_PatternS,patternstr,0);
  lv_label_set_text(ui_HomePatternLabel,patternstr);
  LogDebug(pattern);
  float patterns = pattern;
  SendCommand(PATTERN, patterns, OSSM_ID);
}
```

Rotation equivalents, ready to wire to real widgets once the screen
exists (or to a bench test button/Serial command right now):

```cpp
void rotationStart() {
  SendCommand(ROT_ON, 0.0, OSSM_ID);
}

void rotationStop() {
  SendCommand(ROT_OFF, 0.0, OSSM_ID);
}

// value: 0-100, same convention as the existing speed control
void rotationSetSpeed(float percent) {
  SendCommand(ROT_SPEED, percent, OSSM_ID);
}

// index: into rotationPatternTable on the mainboard - keep this in sync
// with the mainboard's rotationPatternTable order (RotationPattern.h):
// 0 = Steady, 1 = Sweep, 2 = Escalation, 3 = Random Bursts
void rotationSetPattern(int index) {
  SendCommand(ROT_PATTERN, (float)index, OSSM_ID);
}
```

The pattern *names* are known to the mainboard only (`RotationPattern.h`'s
`getName()`) - there's no over-the-air way to ask the mainboard "what are
your pattern names" today (the stroke side doesn't do this either; its
roller's option list, `ui_PatternS`, is hardcoded to match `patternTable[]`
order by convention, not queried). The rotation pattern roller will need
the same hardcoded-list-matches-firmware-order convention: whatever
strings go into the SquareLine roller widget must be typed in the same
order as `rotationPatternTable[]` in `RotationPattern.h`, kept in sync by
hand when patterns are added or reordered.

## 4. Receiving rotation state (mirrors line ~346 `pattern = incomingcontrol.esp_pattern;`)

Wherever incoming stroke state is unpacked from `incomingcontrol` (used to
keep the remote's displayed values in sync after a reconnect/heartbeat),
add the rotation fields alongside:

```cpp
    pattern = incomingcontrol.esp_pattern;
    LogDebug(pattern);
    // add:
    float rotationSpeed = incomingcontrol.esp_rotation_speed;
    int rotationPattern = (int)incomingcontrol.esp_rotation_pattern;
```

No display widgets exist yet to show these - stash them in module-scope
variables now (as above) so the screen work later just wires labels to
variables that are already being kept current, the same shape as `pattern`
today.

## What's still open

- The actual LVGL screen: a speed control + pattern roller for rotation,
  laid out in SquareLine Studio and exported, mirroring `ST_UI_PATTERN`
  and the `ui_PatternS` roller. This doc deliberately stops short of that -
  it's UI/UX layout work, not firmware logic, and worth doing once the
  hardware side (mainboard build steps 7-9) is confirmed working so you're
  not designing a screen for an axis you haven't seen spin yet.
- Whether rotation gets its own top-level screen/menu state (a
  `ROTATION_MENUE` alongside `PATTERN_MENUE`) or lives on the existing
  pattern screen as a second set of controls - a layout decision, not
  answered here.
