# New Rotation Patterns — Integration Notes

Companion to the updated `RotationPattern.h`, `RotationEngine.h`, and
`RotationEngine.cpp` (2026-09-10). Covers what has to change elsewhere to
actually use the 11 new patterns, and what's still unverified.

Table went from 5 patterns to 16 (later 17, see section 6):

```
0  Steady
1  Sweep
2  Escalation
3  Random Bursts
4  Screw Sync
5  Stutter Sync              <- new
6  Bottom Hold Sync          <- new
7  Depth Surge               <- new (needs position feed)
8  Threshold Burst           <- new (needs position feed)
9  Tempo Match                <- new (needs speed feed)
10 Snap Reverse               <- new
11 Alternating Sweep          <- new
12 Random Direction Bursts    <- new
13 Metronome Flip             <- new
14 Drunken Walk               <- new
15 Escalating Reversal        <- new
```

("Independent Drift" from the brainstorm isn't a class — it's Sweep or
Escalation retuned with a long period and shallow minFraction. No table
slot needed.)

## 1. Mandatory: update the M5 remote's pattern roller

`M5-REMOTE-ROTATION-SCREEN.md` section 3c hardcodes:

```cpp
lv_roller_set_options(ui_RotationPatternS,
                      "Steady\nSweep\nEscalation\nRandom Bursts\nScrew Sync",
                      LV_ROLLER_MODE_NORMAL);
```

This has to become all 17 names, in the exact table order above (see
section 6 for the final 17th entry):

```cpp
lv_roller_set_options(ui_RotationPatternS,
                      "Steady\nSweep\nEscalation\nRandom Bursts\nScrew Sync\n"
                      "Stutter Sync\nBottom Hold Sync\nDepth Surge\n"
                      "Threshold Burst\nTempo Match\nSnap Reverse\n"
                      "Alternating Sweep\nRandom Direction Bursts\n"
                      "Metronome Flip\nDrunken Walk\nEscalating Reversal\n"
                      "Depth Trigger Spin",
                      LV_ROLLER_MODE_NORMAL);
```

Same "no over-the-air name query, hardcoded list matches firmware order by
convention" caveat as before — miss this edit and the remote's selector
and the mainboard's table silently disagree about what index N means.

## 2. Mandatory (for 3 patterns only): feed position/speed into the engine

Depth Surge, Threshold Burst, and Tempo Match need `RotationEngine` fed
raw stroke position/speed — direction alone (already wired) isn't enough.
`StrokeEngine::registerTelemetryCallback()` already hands both to
`strokeTelemetryCallback()` in `main.cpp` (see
`MAINBOARD-ROTATION-INTEGRATION.md` section 4a) — that function currently
only uses `position` for direction-delta and drops `speed` entirely. Add:

```cpp
void strokeTelemetryCallback(float position, float speed, bool clipping) {
  if (haveLastStrokeTelemetryPosition) {
    Rotator.setSyncedStrokeDirection(position > lastStrokeTelemetryPosition);
  }
  Rotator.setSyncedStrokePosition(position);   // <-- add
  Rotator.setSyncedStrokeSpeed(speed);          // <-- add
  lastStrokeTelemetryPosition = position;
  haveLastStrokeTelemetryPosition = true;
}
```

Cheap to call every time regardless of which rotation pattern is active —
same reasoning as the existing direction feed.

## 3. NOT yet verified — do this before trusting Depth Surge / Threshold Burst / Tempo Match

`RotationEngine`'s new position/speed normalization ranges
(`_strokePositionMin/Max`, `_strokeSpeedReference`) are placeholders:

```cpp
float _strokePositionMin = 0.0f;
float _strokePositionMax = 150.0f;    // PLACEHOLDER
float _strokeSpeedReference = 100.0f; // PLACEHOLDER
```

Neither this session nor the prior hardware/integration work confirmed
`StrokeEngine`'s actual position units (presumably mm, tied to
`Stroker.physicalTravel` / the configured `DEPTH`/`STROKE` window) or
speed units. Before using the three position/speed-linked patterns:

1. Log `position`/`speed` from `strokeTelemetryCallback` over USB during a
   normal stroke cycle at a known depth/stroke setting, and read off the
   actual min/max your rig produces.
2. Call `Rotator.setStrokePositionRange(observedMin, observedMax)` in
   `setup()` with those numbers (after `Rotator.begin()`, same spot the
   other rotation config calls live — see `MAINBOARD-ROTATION-INTEGRATION.md`
   section 3g).
3. `Rotator.setStrokeSpeedReference(...)` doesn't need a "true" ceiling —
   it's a feel parameter. Start with the max `speed` value you logged and
   adjust until Tempo Match's fast-stroke/fast-spin relationship feels
   proportional rather than pegged at 1.0 immediately or barely moving.

Until step 1–2 happen, Depth Surge/Threshold Burst will run but their
position gating is meaningless (denominator is a guess). Tempo Match
degrades more gracefully — it's just under- or over-sensitive until
`_strokeSpeedReference` is tuned, not wrong in kind.

## 4. Example tuning calls (optional — defaults are reasonable starting points)

Same spot as the existing pattern-tuning calls in `setup()`
(`MAINBOARD-ROTATION-INTEGRATION.md` doesn't show this pattern today
since the original 5 patterns' constructors don't need per-instance
tuning beyond defaults — but if you want to hand-tune before first bench
run, do it right after `Rotator.begin()`):

```cpp
// Cast is safe the same way setScrewSyncInverted() does it internally -
// name-matched against rotationPatternTable, not exposed as a public
// engine passthrough for every new pattern (that'd be a lot of one-off
// methods on RotationEngine). If you want these tunable at runtime later
// rather than compile-time, add passthroughs the same shape as
// setScrewSyncInverted().
```

Realistically: leave defaults alone for the first bench pass on every new
pattern, same as the original 5 shipped without per-instance tuning calls
in the integration doc. Revisit specific constants (burst windows, flip
intervals, hold times) after feeling them on the actual hardware.

## 5. Motor-safety summary — which new patterns need what

| Pattern | Reverses on | Needs `setStrokeSyncSafe()` guard | Needs bench-verify before trusting defaults |
|---|---|---|---|
| Stutter Sync | stroke phase (throttled) | Yes — inherited automatically, same as Screw Sync | No |
| Bottom Hold Sync | stroke phase | Yes — inherited automatically | No |
| Depth Surge | never (manual only) | No | Yes — position range (§3) |
| Threshold Burst | never (manual only) | No | Yes — position range (§3) |
| Tempo Match | never (manual only) | No | Yes — speed reference (§3) |
| Snap Reverse | stroke phase | Yes — inherited automatically | Speed-spike multiplier, see class comment |
| Alternating Sweep | own timer | No — self-triggered | No, defaults sit well above the 250ms floor |
| Random Direction Bursts | own timer | No — self-triggered | No at defaults; check margin if windows are shortened |
| Metronome Flip | own timer | No — self-triggered | No at defaults; don't go below ~500ms untested |
| Drunken Walk | own timer | No — self-triggered | No at defaults; check margin if `_dirMinMs` is lowered |
| **Escalating Reversal** | own timer, rate escalates | No — self-triggered, but has its own internal floor (`MIN_SAFE_HOLD_MS`) | **Yes — explicitly flagged in the class comment, bench-verify before relying on it** |

The three sync-based new patterns (Stutter Sync, Bottom Hold Sync, Snap
Reverse) all get the `setStrokeSyncSafe()`/vibration-pattern guard for
free — they're built on the exact same `_strokeSyncSafe` check as Screw
Sync, no changes needed to `main.cpp`'s
`isVibrationHeavyStrokePattern()`/`Rotator.setStrokePatternSafeForSync()`
wiring from `MAINBOARD-ROTATION-INTEGRATION.md` section 5.

## 6. NEW — "Depth Pulse" / "Depth Trigger Spin" combo (2026-09-10, finalized 2026-09-11)

Two purpose-built patterns, choreographed together via shared cross-axis
feeds (not direct messaging between them):

**The full cycle:**
1. Stroke pushes in smoothly; rotation spins at the knob-controlled
   Speed the whole time (same as Steady).
2. The instant the stroke axis reaches full depth: Depth Pulse starts
   vibrating in place, and Depth Trigger Spin simultaneously opens a
   burst window — speed jumps to the machine's TRUE maximum (bypassing
   the Speed knob entirely), and direction flips repeatedly at even
   intervals for the duration of the window.
3. Both the vibration hold and the burst window last exactly the same
   length — a value computed from the shared Sensation slider, using
   the identical formula and default range (1.0s–30.0s) on both
   patterns. Neither pattern messages the other directly; they just
   compute the same number from the same input.
4. When the window closes: Depth Pulse pulls smoothly all the way out;
   Depth Trigger Spin reverts to knob-controlled speed, holding
   whichever direction the last flip left it in.
5. Repeats on the next stroke cycle.

**Rotation pattern table grows from 16 to 17.** `Depth Trigger Spin` is
appended as index 16. The M5 remote's rotation roller string (section 1
above) needs one more entry at the end (already reflected there).

**Stroke pattern table grows from 9 to 10.** `Depth Pulse` (see
`patches/StrokePattern_DepthPulse.h`) needs adding to
`lib/StrokeEngine/src/pattern.h`'s `patternTable[]`, and — since the
stroke pattern roller was just fixed to correctly list 9 real names (see
the earlier checkpoint) — that fix needs one more entry too:

```cpp
lv_roller_set_options(ui_PatternS,
                      "Simple Stroke\nTeasing or Pounding\nRobo Stroke\nHalf'n'Half\n"
                      "Deeper\nStop'n'Go\nInsist\nJack Hammer\nStroke Nibbler\n"
                      "Depth Pulse",
                      LV_ROLLER_MODE_NORMAL);
```

Check `patternstr[20]`'s buffer margin again if you rename anything —
"Depth Pulse" (11 chars) fits comfortably, no issue there.

**Third cross-axis feed needed: Sensation.** Depth Trigger Spin needs
`RotationEngine::setSyncedStrokeSensation()` called wherever the
mainboard currently applies an incoming Sensation value to `Stroker` —
this project's docs don't show that exact call site (only the M5
remote's *send* side, `SendCommand(SENSATION, sensation, OSSM_ID)` in
`OSSM-M5-Remote/src/main.cpp`), so find the mainboard's Sensation
command handler and add the Rotator call alongside it:

```cpp
// wherever the mainboard currently does something like:
//   Stroker.setSensation(incomingSensation);
Rotator.setSyncedStrokeSensation(incomingSensation);  // <-- add
```

**Fourth requirement: `RotationEngine::setMaxRpm()` must be called
during setup, same as always** — but now this also matters for a new
reason. Depth Trigger Spin's burst phase reads `_maxRpmCeiling`
(inherited from the `RotationPattern` base class via the new
`setMaxRpmCeiling()` hook), which is only populated when
`setMaxRpm()` runs — it now propagates the raw ceiling to every pattern
in the table, not just storing it privately in the engine. If
`setMaxRpm()` is never called, Depth Trigger Spin's burst phase will
spin at 0 RPM instead of true max. This was already a required setup
call before (for the Speed-knob-scaled value every pattern uses); it
just now does double duty.

**How the "flip every N seconds" math actually works, worked example:**
Depth Trigger Spin does NOT run an independent free-running clock, and
does NOT flip only once per window. Within a single burst window, it
flips repeatedly at a fixed sub-interval:

```
window length (seconds) = Sensation mapped across setDurationRange()
                           (default 1.0s-30.0s) - identical formula and
                           default range to Depth Pulse's own hang time
flip interval            = window length x (setReversalIntervalPercentage()
                            / 100), default percentage 50
```

Worked example: a 5-second window (from whatever Sensation value produces
5s in the configured range) with the percentage at 50 flips direction
every 2.5 seconds — twice within the 5-second window. At 25%, every 1.25
seconds — four times within the window. The percentage is a firmware
setting (`setReversalIntervalPercentage()`), not a live control — there's
no second Sensation-style knob on the rotation screen; the rotation
pattern's own timing is this percentage applied to the Sensation-derived
window length both patterns already share, not a separate slider.

**Safety floor — `setMinimumIntervalMs()`, default 500ms.** The
computed flip interval (window length × percentage) is never allowed to
go below this, regardless of how `setDurationRange()` /
`setReversalIntervalPercentage()` are configured. This sits on top of —
not instead of — `RotationEngine`'s own global 250ms
`setMinReversalIntervalMs()` backstop: the floor here stops the pattern
from ever *requesting* a too-fast interval in the first place (a
predictable, intentional interval), rather than relying on the global
backstop to silently defer/hold a reversal that arrives too soon (an
uneven, unpredictable one). The 500ms default isn't a spec pulled from
the Sanyoudian datasheet (not in hand) — bench-verify before relying on
intervals close to it, and definitely before lowering it.

**Design assumption flagged, not yet confirmed:** when a burst window
closes, direction is left wherever the last flip landed it — it is NOT
reset to the manual Reverse checkbox's direction. This matches every
other direction-holding pattern's convention in this file, but if the
intent was for the post-withdrawal "simple rotation" phase to always
resume in a specific (checkbox) direction rather than whatever the burst
happened to end on, that's a small follow-up change to `nextTarget()`'s
window-close branch.

**Sensation does double duty whenever this combo is active.** Depth
Pulse and Depth Trigger Spin both derive their window length from the
same Sensation value by default — turning Sensation up lengthens the
vibration hold AND the burst window (and therefore, proportionally, the
time between flips) together. Pairing Depth Trigger Spin with a
*different* stroke pattern still feeds Sensation into its window-length
calculation regardless of what that stroke pattern locally does with
Sensation — worth knowing if the combination feels like Sensation is
affecting more than expected.

**No real revolution counting.** `RotationEngine` has no encoder or step
counting — it's open-loop frequency generation (see its own header
comment). The flip interval and window length are both straightforward
timed durations; however many physical revolutions happen during any
given interval depends on the current speed and isn't tracked.

**Position-range calibration still applies.** Depth Trigger Spin's
"reached full depth" trigger depends on the same uncalibrated position
range as Depth Surge/Threshold Burst (section 3 above) — until that's
measured, the trigger may fire early, late, or not reliably.
