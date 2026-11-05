/**
 *   Patterns for the CyborgTop rotation axis.
 *   Deliberately mirrors the structure of StrokeEngine's pattern.h
 *   (lib/StrokeEngine/src/pattern.h): a base class every pattern derives
 *   from, plus a static table the engine indexes into by selection.
 *
 *   Key difference from StrokeEngine patterns: the stroke axis calls
 *   nextTarget(index) once per completed stroke, because a stroke is a
 *   discrete, position-bounded event. Rotation has no endstops and no
 *   discrete "stroke" - it just spins - so nextTarget() here is called on
 *   a fixed timer tick (see RotationEngine::update()) and takes the
 *   current millis() instead of a stroke index. That's the only
 *   structural difference; everything else (a name, a table, "add a new
 *   pattern = write a routine + add it to the table") is the same shape.
 *
 *   Adding a new pattern: copy one of the classes below, override
 *   nextTarget(), add it to rotationPatternTable[]. Reflash. Same
 *   workflow as adding a StrokeEngine stroke pattern.
 *
 *   ==========================================================================
 *   2026-09-10 update: added setStrokePosition()/setStrokeSpeed() hooks
 *   (normalized 0-1 fractions) alongside the existing setStrokeDirection()/
 *   setStrokeSyncSafe(). These feed the new position/speed-linked patterns
 *   below (Depth Surge, Threshold Burst, Tempo Match). See
 *   RotationEngine.h/.cpp for the matching engine-side additions
 *   (setSyncedStrokePosition/setSyncedStrokeSpeed + the range-normalization
 *   config) and PATTERN-INTEGRATION-NOTES.md for what main.cpp's
 *   strokeTelemetryCallback needs to feed them - the normalization ranges
 *   are placeholders that MUST be confirmed against Stroker's actual
 *   depth/speed units before these three patterns are trustworthy.
 *   ==========================================================================
 *
 *   ==========================================================================
 *   2026-09-10 update #2: added Depth Trigger Spin (rotation pattern 16)
 *   and its setStrokeSensation() hook, paired with a new stroke-side
 *   pattern (DaddyBearRyte_StrokePattern_DepthPulse.h, not in this file -
 *   see PATTERN-INTEGRATION-NOTES.md for the full combo writeup).
 *   ==========================================================================
 *
 *   ==========================================================================
 *   2026-09-11 update: Depth Trigger Spin redesigned to match the actual
 *   spec - a bounded burst WINDOW (same length as Depth Pulse's vibration
 *   hold) during which speed bypasses the Speed knob entirely (true max,
 *   via the new setMaxRpmCeiling() base-class hook) and direction flips
 *   repeatedly at even intervals, then reverts cleanly to knob-controlled
 *   speed when the window closes. Replaces the prior single-flip-at-the-
 *   end design. See the class's own header comment for the full
 *   walkthrough.
 *   ==========================================================================
 */

#pragma once

#include <Arduino.h>
#include <math.h>

#ifndef ROT_STRING_LEN
  #define ROT_STRING_LEN 32   // no pattern name should exceed this
#endif

/**************************************************************************/
/*!
  @brief  What a rotation pattern hands back each tick: target speed,
  direction, and whether this tick has nothing new to say (skip = true,
  engine holds last commanded state - used for timed pauses).
*/
/**************************************************************************/
typedef struct {
    float rpm;       //!< Target speed in RPM. 0 = stopped (pulses cease).
    bool  reverse;    //!< false = forward (as wired at bring-up), true = reverse
    bool  skip;       //!< true: no change this tick, engine holds last output
} rotationMotionParameter;

/**************************************************************************/
/*!
  @class RotationPattern
  @brief Base class every rotation pattern derives from. A pattern is
  self-contained and time-based - it knows nothing about GPIO, LEDC, or
  the Sanyoudian driver. RotationEngine is the only thing that touches
  hardware; patterns just answer "what should the axis be doing right
  now" given how long the pattern has been active and the current base
  speed (the % speed knob value from the M5 remote, already converted to
  RPM by RotationEngine::setSpeedPercent()).
*/
/**************************************************************************/
class RotationPattern {

    public:
        //! Constructor
        /*!
          @param str Name of the pattern, shown on the M5 remote pattern
          selector - same UX as the stroke pattern list.
        */
        RotationPattern(const char *str) { strncpy(_name, str, ROT_STRING_LEN - 1); _name[ROT_STRING_LEN - 1] = '\0'; }

        //! The current base speed to modulate, in RPM. Comes from the M5
        //! remote's speed control (percent) already scaled by RotationEngine
        //! against the configured max RPM.
        virtual void setBaseSpeed(float rpm) { _baseRpm = rpm; }

        //! The engine's TRUE speed ceiling (RotationEngine's own
        //! setMaxRpm() value), independent of the Speed knob. Most
        //! patterns never need this - setBaseSpeed() above is already the
        //! knob-scaled value every other pattern uses as "full speed."
        //! This exists for patterns that deliberately need to bypass the
        //! knob and spin at the machine's actual maximum regardless of
        //! what the user has Speed set to (Depth Trigger Spin's burst
        //! phase is the only one that does this today). Pushed to every
        //! pattern whenever RotationEngine::setMaxRpm() is called, not
        //! just the active one, so it's always current if a pattern that
        //! needs it gets selected later.
        virtual void setMaxRpmCeiling(float maxRpm) { _maxRpmCeiling = maxRpm; }

        //! Overdrive peak scale pushed by RotationEngine: 1.0 when overdrive
        //! is off, ROTATION_OVERDRIVE_FACTOR when on. Patterns that have a
        //! "peak" (burst top, top of a speed walk, Depth Trigger Spin's burst)
        //! multiply that peak by this. Everything below the peak stays
        //! dial-relative.
        virtual void setPeakScale(float scale) { _peakScale = (scale < 1.0f) ? 1.0f : scale; }

        //! Rotation Sensation, -1.0 to 1.0, pushed by RotationEngine from the
        //! remote's rotation Sensation dial. Acts on: Screw Sync, Threshold Burst,
        //! Alternating Sweep, Random Bursts, Metronome Flip, Depth Trigger Spin.
        //! Convention: negative = faster / shorter / more frequent, positive =
        //! slower / longer / less frequent, 0 = the pattern's default behavior.
        virtual void setRotationSensation(float fraction) { _rotationSensationFraction = constrain(fraction, -1.0f, 1.0f); }

        //! Fed by RotationEngine every tick from the stroke axis's telemetry
        //! (see main.cpp's strokeTelemetryCallback). Most patterns ignore
        //! this entirely - it exists for patterns like ScrewSync that
        //! synchronize rotation to stroke phase. Same idea as StrokeEngine's
        //! Pattern::setSensation(): a parameter every pattern receives,
        //! only some act on.
        /*!
          @param extending true if the stroke axis's most recently commanded
          leg moves it deeper (toward Stroker's DEPTH); false if it's
          retracting (toward DEPTH-STROKE).
        */
        virtual void setStrokeDirection(bool extending) { _strokeExtending = extending; }

        //! Fed by RotationEngine every tick: whether the currently-active
        //! STROKE pattern is safe to phase-sync rotation direction against.
        //! False while StrokeEngine is running a vibration-heavy stroke
        //! pattern (see main.cpp's isVibrationHeavyStrokePattern()) - those
        //! command many direction reversals per second, which is fine for
        //! the stroke axis's own reciprocating motion but is not something
        //! to blindly mirror onto a second motor. Most patterns ignore this;
        //! only patterns that reverse in response to stroke phase (ScrewSync,
        //! Stutter Sync, Bottom Hold Sync, Snap Reverse) act on it.
        virtual void setStrokeSyncSafe(bool safe) { _strokeSyncSafe = safe; }

        //! The user's manually-chosen base direction (M5 remote's rotation
        //! screen "Reverse" checkbox), false = forward. Honored by every
        //! pattern that doesn't manage its own direction state internally
        //! (phase-synced patterns like ScrewSync, and self-contained
        //! direction-changing patterns like Metronome Flip, ignore this
        //! after their first reset() - see each class's comment).
        virtual void setBaseDirection(bool reverse) { _baseDirection = reverse; }

        //! Fed by RotationEngine every tick, normalized 0.0-1.0: how far
        //! through its configured travel the stroke axis currently sits
        //! (0 = fully retracted, 1 = full depth). Only meaningful to
        //! position-linked patterns (Depth Surge, Threshold Burst); every
        //! other pattern ignores it. See RotationEngine::setSyncedStrokePosition()
        //! and the range-normalization note there - the 0-1 mapping depends
        //! on Stroker's configured DEPTH/STROKE settings, confirmed at bench.
        virtual void setStrokePosition(float positionFraction) { _strokePositionFraction = constrain(positionFraction, 0.0f, 1.0f); }

        //! Fed by RotationEngine every tick, normalized 0.0-1.0: stroke
        //! axis's current speed as a fraction of a configured reference
        //! max speed. Only meaningful to Tempo Match; every other pattern
        //! ignores it. See RotationEngine::setSyncedStrokeSpeed() - the
        //! reference max is a tunable, not a physical constant, and needs
        //! bench calibration to feel right (see PATTERN-INTEGRATION-NOTES.md).
        virtual void setStrokeSpeed(float speedFraction) { _strokeSpeedFraction = constrain(speedFraction, 0.0f, 1.0f); }

        //! Fed by RotationEngine every time the stroke axis's Sensation
        //! slider changes, normalized -1.0 to 1.0 (Sensation's own range is
        //! a fixed StrokeEngine spec, -100 to 100 - no bench calibration
        //! needed here, unlike position/speed above). Only meaningful to
        //! Depth Trigger Spin; every other pattern ignores it. See
        //! RotationEngine::setSyncedStrokeSensation().
        virtual void setStrokeSensation(float sensationFraction) { _strokeSensationFraction = constrain(sensationFraction, -1.0f, 1.0f); }

        //! Fed by RotationEngine every tick when a live position provider is
        //! registered (see RotationEngine::setLivePositionProvider()),
        //! normalized 0.0-1.0 like setStrokePosition(), but from the stepper's
        //! ACTUAL position instead of the commanded move targets. Only Depth
        //! Trigger Spin reads it. Until the first call _haveLivePosition stays
        //! false and that pattern falls back to its fixed burst window.
        virtual void setStrokeLivePosition(float positionFraction) { _strokeLiveFraction = constrain(positionFraction, 0.0f, 1.0f); _haveLivePosition = true; }

        //! Called once when this pattern becomes the active pattern, so it
        //! can zero its own phase/timers instead of resuming mid-cycle from
        //! a previous activation.
        virtual void reset(unsigned long nowMillis) { _phaseStartMillis = nowMillis; }

        //! Retrieves the name of a pattern
        char *getName() { return _name; }

        //! Compute the next target. Called every RotationEngine tick
        //! (~30ms, see RotationEngine.cpp) - NOT once per revolution.
        /*!
          @param nowMillis current millis(), for time-based phases
          @return target speed/direction for the engine to apply
        */
        virtual rotationMotionParameter nextTarget(unsigned long nowMillis) {
            return _nextMove;
        }

    protected:
        char _name[ROT_STRING_LEN];
        float _baseRpm = 0.0f;
        float _maxRpmCeiling = 0.0f;
        float _peakScale = 1.0f;
        float _rotationSensationFraction = 0.0f;
        float _strokeLiveFraction = 0.0f;
        bool _haveLivePosition = false;

        //! Rotation Sensation as a time/rate scale: 1.0 at 0, 0.25 at -1, 4.0 at +1.
        //! Multiply a period by this: negative sensation = shorter period (faster).
        float _sensScale() const { return powf(4.0f, _rotationSensationFraction); }
        bool _baseDirection = false;
        bool _strokeExtending = false;
        bool _strokeSyncSafe = true;
        float _strokePositionFraction = 0.0f;
        float _strokeSpeedFraction = 0.0f;
        float _strokeSensationFraction = 0.0f;
        unsigned long _phaseStartMillis = 0;
        rotationMotionParameter _nextMove = {0.0f, false, false};
};

/**************************************************************************/
/*!
  @brief  Constant speed at the base RPM. No direction changes. This is
  the "just spin" pattern - equivalent to StrokeEngine's SimpleStroke.
*/
/**************************************************************************/
class SteadySpeed : public RotationPattern {
    public:
        SteadySpeed(const char *str) : RotationPattern(str) {}

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm;
            _nextMove.reverse = _baseDirection; // user's manual direction choice
            _nextMove.skip = false;
            return _nextMove;
        }
};

/**************************************************************************/
/*!
  @brief  Smooth speed sweep between a floor fraction and full base speed,
  using a raised-cosine curve so it eases in/out instead of triangle-wave
  snapping at the turnarounds. Direction stays constant. One full cycle
  takes _periodMillis.

  USAGE NOTE - "Independent Drift": there is no separate class for this.
  Select Sweep (or Escalation) and retune it with a long, slow period and
  a shallow minFraction (e.g. setSweepParameters(20000, 0.6)) for a lazy,
  deliberately-not-synced free-running drift. It's the same mechanism,
  just tuned to feel decoupled rather than energetic - not worth a
  duplicate class.
*/
/**************************************************************************/
class OscillatingSweep : public RotationPattern {
    public:
        OscillatingSweep(const char *str) : RotationPattern(str) {}

        //! @param periodMillis time for one full sweep cycle (low->high->low)
        //! @param minFraction  floor of the sweep as a fraction of base speed (0-1)
        void setSweepParameters(unsigned long periodMillis, float minFraction) {
            _periodMillis = periodMillis;
            _minFraction = minFraction;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (_phaseStartMillis == 0) _phaseStartMillis = nowMillis;
            float phase = fmodf((float)(nowMillis - _phaseStartMillis), (float)_periodMillis) / (float)_periodMillis; // 0..1
            float shape = 0.5f * (1.0f - cosf(2.0f * PI * phase)); // 0..1, smooth
            _nextMove.rpm = _baseRpm * (_minFraction + (1.0f - _minFraction) * shape);
            _nextMove.reverse = _baseDirection; // user's manual direction choice
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _periodMillis = 6000;
        float _minFraction = 0.3f;
};

/**************************************************************************/
/*!
  @brief  Linear ramp from a floor fraction up to full base speed over
  _rampMillis, holds at full speed for _holdMillis, then resets to the
  floor and repeats. An "edging" escalation shape.
*/
/**************************************************************************/
class RampingEscalation : public RotationPattern {
    public:
        RampingEscalation(const char *str) : RotationPattern(str) {}

        //! @param rampMillis time to climb from floor to full speed
        //! @param holdMillis time to hold at full speed before resetting
        //! @param minFraction floor of the ramp as a fraction of base speed (0-1)
        void setRampParameters(unsigned long rampMillis, unsigned long holdMillis, float minFraction) {
            _rampMillis = rampMillis;
            _holdMillis = holdMillis;
            _minFraction = minFraction;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (_phaseStartMillis == 0) _phaseStartMillis = nowMillis;
            unsigned long cycleMillis = _rampMillis + _holdMillis;
            unsigned long elapsed = (nowMillis - _phaseStartMillis) % cycleMillis;

            float fraction = (elapsed < _rampMillis)
                ? (float)elapsed / (float)_rampMillis   // 0..1 climbing
                : 1.0f;                                  // holding at top

            _nextMove.rpm = _baseRpm * (_minFraction + (1.0f - _minFraction) * fraction);
            _nextMove.reverse = _baseDirection; // user's manual direction choice
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _rampMillis = 15000;
        unsigned long _holdMillis = 2000;
        float _minFraction = 0.2f;
};

/**************************************************************************/
/*!
  @brief  Randomized bursts of full base speed against a low resting
  speed, with randomized burst/rest durations pulled fresh each time a
  window elapses. Some bursts also switch direction (merged in from the
  old Random Direction Bursts): the roll happens once per burst-start, so
  a single burst is always one consistent direction. The Reverse checkbox
  only seeds the STARTING direction at reset(); the pattern owns direction
  after that.

  Rotation Sensation sets the gap (rest) length: 0 = the configured rest
  windows, negative = shorter gaps (bursts come more often), positive =
  longer gaps. Scale is 0.25x to 4x.

  Reversal-rate note: worst case (every burst flips) is roughly one
  reversal per burst+rest cycle. With the default windows (400-1500ms
  burst, 800-3000ms rest) at the shortest gap scale (0.25x) that floors
  around 600ms, above the engine's 250ms backstop.
*/
/**************************************************************************/
class RandomizedBursts : public RotationPattern {
    public:
        RandomizedBursts(const char *str) : RotationPattern(str) {}

        //! Tune burst/rest windows (all in ms) and the resting-speed fraction
        void setBurstParameters(unsigned long burstMinMs, unsigned long burstMaxMs,
                                 unsigned long restMinMs, unsigned long restMaxMs,
                                 float restFraction) {
            _burstMinMs = burstMinMs; _burstMaxMs = burstMaxMs;
            _restMinMs = restMinMs;   _restMaxMs = restMaxMs;
            _restFraction = restFraction;
        }

        void setDirectionFlipChancePercent(uint8_t percent) { _flipChancePercent = min((uint8_t)100, percent); }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _bursting = false;
            _nextEventMillis = 0; // forces a fresh random window on the next tick
            _reverse = _baseDirection; // seed from manual choice; not re-read after this
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (nowMillis >= _nextEventMillis) {
                _bursting = !_bursting;
                unsigned long window;
                if (_bursting) {
                    window = random(_burstMinMs, _burstMaxMs);
                } else {
                    float gapScale = _sensScale();
                    unsigned long lo = (unsigned long)((float)_restMinMs * gapScale);
                    unsigned long hi = (unsigned long)((float)_restMaxMs * gapScale);
                    if (hi <= lo) hi = lo + 1;
                    window = random(lo, hi);
                }
                _nextEventMillis = nowMillis + window;
                if (_bursting && ((unsigned long)random(0, 100) < _flipChancePercent)) {
                    _reverse = !_reverse;
                }
            }
            _nextMove.rpm = _bursting ? (_baseRpm * _peakScale) : (_baseRpm * _restFraction);
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _bursting = false;
        unsigned long _nextEventMillis = 0;
        unsigned long _burstMinMs = 400, _burstMaxMs = 1500;
        unsigned long _restMinMs = 800, _restMaxMs = 3000;
        float _restFraction = 0.25f;
        uint8_t _flipChancePercent = 35;
        bool _reverse = false;
};

/**************************************************************************/
/*!
  @brief  Screw in / unscrew out. Rotation direction tracks the stroke
  axis's phase: forward while the stroke is extending (moving toward
  DEPTH), reverse while it's retracting (moving toward DEPTH-STROKE).
  Speed stays constant at base speed - only direction changes. Needs
  RotationEngine::setSyncedStrokeDirection() to be fed from StrokeEngine's
  telemetry callback (see MAINBOARD-ROTATION-INTEGRATION.md); with no
  feed it just sits at whatever setStrokeDirection() was last called
  with (default: forward).

  Motor-safety note: this pattern will happily reverse direction as often
  as the stroke axis asks it to. Paired with a vibration-heavy stroke
  pattern (Jack Hammer, Stroke Nibbler - many micro-legs per second, each
  a direction flip) that's a lot of rapid reversals for the rotation
  motor/driver to absorb. Two independent guards exist against that, both
  in RotationEngine, not in this class - see "Motor-safety override" in
  MAINBOARD-ROTATION-INTEGRATION.md section 4:
    1. setStrokeSyncSafe(false) makes this pattern hold its current
       direction instead of chasing _strokeExtending - main.cpp sets this
       automatically based on which stroke pattern is active.
    2. RotationEngine enforces a minimum interval between actual direction
       reversals regardless of what any pattern requests, as a backstop.
*/
/**************************************************************************/
class ScrewSync : public RotationPattern {
    public:
        ScrewSync(const char *str) : RotationPattern(str) {}

        //! Flip if "extending" spins the wrong way on the bench - same
        //! bench-test-and-flip idiom as RotationEngine's invertDirection,
        //! just scoped to this one pattern instead of the whole axis,
        //! since "which way is screwing in" is a property of the physical
        //! thread, not the motor wiring.
        void setInverted(bool inverted) { _inverted = inverted; }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _haveLastExtending = false;
            _reversalCount = 0;
        }

        //! Rotation Sensation sets how many stroke reversals pass between
        //! direction flips. 0 = every reversal (1:1, the original Screw
        //! Sync). Either way from 0 the delay grows: |sens| 1.0 = every 8th
        //! reversal (Stutter Sync is every 2nd). Sign has no effect here.
        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm;
            unsigned int everyNth = 1 + (unsigned int)(fabsf(_rotationSensationFraction) * 7.0f + 0.5f);
            if (_strokeSyncSafe) {
                if (everyNth <= 1) {
                    _committedReverse = _inverted ? _strokeExtending : !_strokeExtending;
                } else if (_haveLastExtending && (_strokeExtending != _lastExtending)) {
                    _reversalCount++;
                    if (_reversalCount % everyNth == 0) {
                        // Toggle rather than re-derive from the stroke phase: with an
                        // even N the stroke phase is identical at every Nth reversal,
                        // so re-deriving would never change direction.
                        _committedReverse = !_committedReverse;
                    }
                }
                _lastExtending = _strokeExtending;
                _haveLastExtending = true;
            }
            // else: hold the current direction rather than chase
            // _strokeExtending, and do not count reversals. It's being
            // toggled faster than is safe to mirror (a vibration-heavy
            // stroke pattern is active, or safety was otherwise withheld -
            // see RotationEngine::setStrokePatternSafeForSync()). This
            // intentionally does NOT stop rotation, just freezes direction
            // until a stroke pattern flagged safe is selected again.
            _nextMove.reverse = _committedReverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _inverted = false;
        bool _committedReverse = false;
        bool _lastExtending = false;
        bool _haveLastExtending = false;
        unsigned int _reversalCount = 0;
};

/**************************************************************************/
/*!
  @brief  Like Screw Sync, but only commits a direction flip on every
  Nth stroke-axis reversal instead of every one - "screw in for two
  strokes, unscrew for two strokes" instead of following every single
  stroke. Halves (or divides by N) the reversal rate for a slower, more
  deliberate cadence while still feeling phase-derived rather than
  arbitrary.

  Same motor-safety contract as Screw Sync: while setStrokeSyncSafe(false)
  (vibration-heavy stroke pattern active), holds current direction rather
  than counting reversals at all - a stroke reversal that happens during
  an unsafe window is simply not counted, not queued for later either.
*/
/**************************************************************************/
class StutterSync : public RotationPattern {
    public:
        StutterSync(const char *str) : RotationPattern(str) {}

        //! Commit a direction flip every Nth stroke-axis reversal. N=1
        //! degenerates to plain Screw Sync behavior; default N=2.
        void setReversalDivider(uint8_t everyNth) { _everyNth = max((uint8_t)1, everyNth); }

        //! Same meaning as ScrewSync::setInverted() - confirm on the bench.
        void setInverted(bool inverted) { _inverted = inverted; }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _haveLastExtending = false;
            _reversalCount = 0;
            // Deliberately does NOT reset _committedReverse - re-selecting
            // this pattern shouldn't snap direction, same spirit as
            // ScrewSync holding through an unsafe window.
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm;
            if (_strokeSyncSafe) {
                if (_haveLastExtending && (_strokeExtending != _lastExtending)) {
                    _reversalCount++;
                    if (_reversalCount % _everyNth == 0) {
                        _committedReverse = _inverted ? _strokeExtending : !_strokeExtending;
                    }
                }
                _lastExtending = _strokeExtending;
                _haveLastExtending = true;
            }
            // else: hold _committedReverse and stop counting - same
            // "freeze, don't queue" contract as ScrewSync.
            _nextMove.reverse = _committedReverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        uint8_t _everyNth = 2;
        bool _inverted = false;
        bool _committedReverse = false;
        bool _lastExtending = false;
        bool _haveLastExtending = false;
        unsigned int _reversalCount = 0;
};

/**************************************************************************/
/*!
  @brief  Screw Sync's direction logic, but with a brief full stop right
  at each direction flip before resuming - a "catch and grind" beat at
  the top/bottom of each stroke instead of a clean, continuous handoff.

  Same motor-safety contract as Screw Sync (setStrokeSyncSafe governs
  whether direction tracks the stroke at all). The hold pause itself
  (rpm -> 0, no direction change) is not a "reversal" from the motor's
  perspective and isn't subject to setMinReversalIntervalMs() - only the
  DIR-pin flip that happens at the top of the pause is.
*/
/**************************************************************************/
class BottomHoldSync : public RotationPattern {
    public:
        BottomHoldSync(const char *str) : RotationPattern(str) {}

        //! Same meaning as ScrewSync::setInverted().
        void setInverted(bool inverted) { _inverted = inverted; }

        //! How long to sit at 0 RPM right after each direction flip.
        void setHoldMillis(unsigned long holdMs) { _holdMillis = holdMs; }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _haveLastExtending = false;
            _holdUntilMillis = 0;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (_strokeSyncSafe) {
                bool wantReverse = _inverted ? _strokeExtending : !_strokeExtending;
                if (_haveLastExtending && (wantReverse != _committedReverse)) {
                    _committedReverse = wantReverse;
                    _holdUntilMillis = nowMillis + _holdMillis;
                } else if (!_haveLastExtending) {
                    _committedReverse = wantReverse; // first sample - nothing to catch yet
                }
                _haveLastExtending = true;
            }
            // else: freeze direction AND don't start a new hold - same
            // "nothing external happens" contract as ScrewSync.
            _nextMove.reverse = _committedReverse;
            _nextMove.rpm = (nowMillis < _holdUntilMillis) ? 0.0f : _baseRpm;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _inverted = false;
        unsigned long _holdMillis = 200;
        unsigned long _holdUntilMillis = 0;
        bool _committedReverse = false;
        bool _haveLastExtending = false;
};

/**************************************************************************/
/*!
  @brief  Rotation speed scales with how deep the stroke axis currently
  is - ramps up approaching full depth, eases off retracting. Direction
  is the user's manual choice (Steady's convention) - this pattern never
  reverses on its own, so it has no motor-safety considerations at all.

  Needs RotationEngine::setSyncedStrokePosition() fed from the stroke
  telemetry callback - see PATTERN-INTEGRATION-NOTES.md. With no feed,
  _strokePositionFraction sits at its default 0.0 and this pattern just
  runs at minFraction * baseRpm continuously.
*/
/**************************************************************************/
class DepthSurge : public RotationPattern {
    public:
        DepthSurge(const char *str) : RotationPattern(str) {}

        //! Floor speed (as a fraction of base RPM) at minimum depth.
        void setSurgeParameters(float minFraction) { _minFraction = minFraction; }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm * (_minFraction + (1.0f - _minFraction) * _strokePositionFraction);
            _nextMove.reverse = _baseDirection;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        float _minFraction = 0.15f;
};

/**************************************************************************/
/*!
  @brief  Idles at a low resting speed for most of the stroke, then
  bursts to full base speed only once stroke depth crosses a threshold
  fraction - a gate rather than Depth Surge's continuous ramp. Direction
  is the user's manual choice; no motor-safety considerations (speed
  only, same as Depth Surge).

  Same feed requirement as Depth Surge - RotationEngine::setSyncedStrokePosition().
*/
/**************************************************************************/
class ThresholdBurst : public RotationPattern {
    public:
        ThresholdBurst(const char *str) : RotationPattern(str) {}

        //! @param thresholdFraction  depth fraction (0-1) above which the burst kicks in
        //! @param restFraction       resting speed fraction below threshold
        void setThresholdParameters(float thresholdFraction, float restFraction) {
            _thresholdFraction = thresholdFraction;
            _restFraction = restFraction;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            // Rotation Sensation moves the threshold: negative = shallower
            // (speeds up sooner), positive = deeper (only near full depth),
            // 0 = the configured default.
            float s = _rotationSensationFraction;
            float threshold = (s < 0.0f)
                ? _thresholdFraction + s * (_thresholdFraction - _thresholdMinFraction)
                : _thresholdFraction + s * (_thresholdMaxFraction - _thresholdFraction);
            bool pastThreshold = _strokePositionFraction >= threshold;
            _nextMove.rpm = _baseRpm * (pastThreshold ? 1.0f : _restFraction);
            _nextMove.reverse = _baseDirection;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        float _thresholdFraction = 0.8f;
        float _thresholdMinFraction = 0.3f;   // threshold at Rotation Sensation -1.0
        float _thresholdMaxFraction = 0.98f;  // threshold at Rotation Sensation +1.0
        float _restFraction = 0.2f;
};

/**************************************************************************/
/*!
  @brief  Rotation speed tracks stroke speed: fast strokes spin fast,
  slow/edging strokes spin slow. Direction is the user's manual choice -
  like the two position-linked patterns above, this only ever changes
  speed from the external signal, never direction, so it carries no
  motor-safety concerns.

  Needs RotationEngine::setSyncedStrokeSpeed() fed from the stroke
  telemetry callback, normalized against a configured reference max
  speed - see PATTERN-INTEGRATION-NOTES.md. That reference is a tunable,
  not a known physical constant; expect to adjust it at the bench until
  "fast stroke = fast spin" actually feels proportional.
*/
/**************************************************************************/
class TempoMatch : public RotationPattern {
    public:
        TempoMatch(const char *str) : RotationPattern(str) {}

        //! Floor speed (as a fraction of base RPM) when the stroke axis is
        //! at its slowest / stationary.
        void setTempoParameters(float minFraction) { _minFraction = minFraction; }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm * (_minFraction + (1.0f - _minFraction) * _strokeSpeedFraction);
            _nextMove.reverse = _baseDirection;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        float _minFraction = 0.2f;
};

/**************************************************************************/
/*!
  @brief  Edge-triggered version of Screw Sync: flips direction at the
  moment of each stroke turnaround, same as Screw Sync, but adds a brief
  speed spike right at the flip instead of Screw Sync's flat continuous
  speed - a punchier, more discrete "snap" at each handoff rather than a
  smooth mirror. (Functionally the direction logic IS Screw Sync's -
  RotationEngine only ever writes the DIR pin on an actual change, so
  continuous-tracking and edge-triggered direction are observably
  identical; what differentiates this pattern is the speed spike.)

  Same motor-safety contract as Screw Sync/Stutter Sync/Bottom Hold Sync.

  Tuning note: _snapMultiplier pushes the requested RPM above baseRpm
  (which is already the user's speed-knob value scaled to maxRpm) for
  the duration of the snap. That's a deliberate transient overshoot, not
  a bug - but it means the actual instantaneous frequency asked of the
  Sanyoudian driver can exceed what the speed knob nominally caps at.
  Keep _snapMultiplier modest (1.2-1.5x) unless you've bench-confirmed
  the driver/motor tolerate brief spikes above the configured ceiling at
  the speeds you actually run.
*/
/**************************************************************************/
class SnapReverse : public RotationPattern {
    public:
        SnapReverse(const char *str) : RotationPattern(str) {}

        //! Same meaning as ScrewSync::setInverted().
        void setInverted(bool inverted) { _inverted = inverted; }

        //! @param snapMillis      how long the speed spike lasts after a flip
        //! @param snapMultiplier  peak speed during the spike, as a multiple of baseRpm
        void setSnapParameters(unsigned long snapMillis, float snapMultiplier) {
            _snapMillis = snapMillis;
            _snapMultiplier = snapMultiplier;
        }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _haveLastExtending = false;
            _snapUntilMillis = 0;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (_strokeSyncSafe) {
                bool wantReverse = _inverted ? _strokeExtending : !_strokeExtending;
                if (_haveLastExtending && (wantReverse != _committedReverse)) {
                    _committedReverse = wantReverse;
                    _snapUntilMillis = nowMillis + _snapMillis;
                } else if (!_haveLastExtending) {
                    _committedReverse = wantReverse;
                }
                _haveLastExtending = true;
            }
            _nextMove.reverse = _committedReverse;
            float multiplier = (nowMillis < _snapUntilMillis) ? _snapMultiplier : 1.0f;
            _nextMove.rpm = _baseRpm * multiplier;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _inverted = false;
        unsigned long _snapMillis = 150;
        float _snapMultiplier = 1.4f;
        unsigned long _snapUntilMillis = 0;
        bool _committedReverse = false;
        bool _haveLastExtending = false;
};

/**************************************************************************/
/*!
  @brief  Oscillating Sweep's speed shape, but direction flips every N
  sweep cycles instead of holding constant - speed and direction phases
  run on independent clocks (the sweep's own period vs. the flip
  divider), so they drift relative to each other over time rather than
  locking together. Self-contained - no stroke telemetry involved.

  Ignores setBaseDirection() after the first reset(): like ScrewSync,
  this pattern manages its own direction state internally rather than
  taking the user's manual choice every tick. reset() seeds the starting
  direction FROM the manual choice, so the checkbox still matters at
  pattern-select time, it just isn't re-read every tick afterward.

  No stroke-sync guard needed - reversals are on this pattern's own
  timer, not triggered by anything external. Default flip period (2
  sweep cycles x 6s = ~12s between flips) sits far above the engine's
  250ms floor; if you shorten the sweep period or set flipEveryNCycles
  aggressively, sanity-check the resulting flip interval against that
  floor before relying on it feeling smooth rather than rate-limited.
*/
/**************************************************************************/
class AlternatingSweep : public RotationPattern {
    public:
        AlternatingSweep(const char *str) : RotationPattern(str) {}

        //! Same meaning as OscillatingSweep::setSweepParameters().
        void setSweepParameters(unsigned long periodMillis, float minFraction) {
            _periodMillis = periodMillis;
            _minFraction = minFraction;
        }

        //! Flip direction every N completed sweep cycles. N=1 flips every cycle.
        void setFlipEveryNCycles(uint8_t n) { _flipEveryN = max((uint8_t)1, n); }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _lastCycleIndex = 0;
            _reverse = _baseDirection; // seed from manual choice; not re-read after this
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            unsigned long elapsed = nowMillis - _phaseStartMillis;
            unsigned long cycleIndex = elapsed / _periodMillis;
            if (cycleIndex != _lastCycleIndex) {
                // Rotation Sensation scales the flip count: negative = fewer
                // waves between flips (min every wave), positive = more (up
                // to 4x), 0 = the configured count.
                float s = _rotationSensationFraction;
                float nScale = (s < 0.0f) ? (1.0f + 0.5f * s) : (1.0f + 3.0f * s);
                unsigned long flipN = (unsigned long)((float)_flipEveryN * nScale + 0.5f);
                if (flipN < 1) flipN = 1;
                if (cycleIndex % flipN == 0) {
                    _reverse = !_reverse;
                }
                _lastCycleIndex = cycleIndex;
            }
            float phase = (float)(elapsed % _periodMillis) / (float)_periodMillis;
            float shape = 0.5f * (1.0f - cosf(2.0f * PI * phase));
            _nextMove.rpm = _baseRpm * (_minFraction + (1.0f - _minFraction) * shape);
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _periodMillis = 6000;
        float _minFraction = 0.3f;
        uint8_t _flipEveryN = 2;
        unsigned long _lastCycleIndex = 0;
        bool _reverse = false;
};

/**************************************************************************/
/*!
  @brief  Constant speed, direction flips on a fixed timer - the simplest
  possible direction-changing pattern, no speed shaping at all. Good as
  a calm contrast to the busier patterns, or as a base to copy when
  building something new. Self-contained, no stroke telemetry, no
  stroke-sync guard needed.

  Ignores setBaseDirection() after the first reset(), same as Alternating
  Sweep - the checkbox seeds the starting direction, this pattern owns
  it from there.

  Default 5s interval is far above the 250ms floor. Don't set
  _intervalMillis much below ~500ms without bench-confirming the motor/
  driver are fine with that reversal rate - this pattern has no guard
  of its own beyond the engine's backstop.
*/
/**************************************************************************/
class MetronomeFlip : public RotationPattern {
    public:
        MetronomeFlip(const char *str) : RotationPattern(str) {}

        void setFlipIntervalMillis(unsigned long intervalMs) { _intervalMillis = intervalMs; }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _reverse = _baseDirection;
            _lastFlipMillis = nowMillis;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            // Rotation Sensation scales the beat: negative = faster, positive
            // = slower, 0 = the configured interval (0.25x to 4x).
            unsigned long interval = (unsigned long)((float)_intervalMillis * _sensScale());
            if (nowMillis - _lastFlipMillis >= interval) {
                _reverse = !_reverse;
                _lastFlipMillis = nowMillis;
            }
            _nextMove.rpm = _baseRpm;
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _intervalMillis = 5000;
        unsigned long _lastFlipMillis = 0;
        bool _reverse = false;
};

/**************************************************************************/
/*!
  @brief  Direction and speed each wander independently on their own
  randomized timers, with no correlation between them - direction flips
  at random intervals within a configured range, speed drifts up/down in
  small random steps on a separate timer. Deliberately unpredictable;
  distinct from Random Bursts' pulse-and-rest shape and from Random
  Direction Bursts' burst-gated flips. Self-contained, no stroke
  telemetry, no stroke-sync guard needed.

  Default direction-walk interval (2-9s) sits well above the 250ms floor.
  If you narrow _dirMinMs much below ~500-1000ms, bench-check the actual
  resulting reversal cadence - a random walk's minimum interval, not its
  average, is what can brush up against the engine's backstop.
*/
/**************************************************************************/
class DrunkenWalk : public RotationPattern {
    public:
        DrunkenWalk(const char *str) : RotationPattern(str) {}

        //! Random range (ms) between direction-flip decisions.
        void setDirectionWalkParameters(unsigned long minIntervalMs, unsigned long maxIntervalMs) {
            _dirMinMs = minIntervalMs; _dirMaxMs = maxIntervalMs;
        }

        //! @param stepMs         how often the speed takes a random step
        //! @param minFraction    floor speed fraction the walk won't go below
        //! @param maxStepFraction  largest single step size (fraction of base RPM)
        void setSpeedWalkParameters(unsigned long stepMs, float minFraction, float maxStepFraction) {
            _speedStepMs = stepMs; _minFraction = minFraction; _maxStepFraction = maxStepFraction;
        }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _reverse = _baseDirection;
            _nextDirEventMillis = nowMillis + random(_dirMinMs, _dirMaxMs);
            _nextSpeedStepMillis = nowMillis + _speedStepMs;
            _speedFraction = 0.6f; // start mid-range rather than snapping from 0 on activation
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (nowMillis >= _nextDirEventMillis) {
                _reverse = !_reverse;
                _nextDirEventMillis = nowMillis + random(_dirMinMs, _dirMaxMs);
            }
            if (nowMillis >= _nextSpeedStepMillis) {
                float step = ((float)random(-1000, 1001) / 1000.0f) * _maxStepFraction;
                _speedFraction = constrain(_speedFraction + step, _minFraction, 1.0f);
                _nextSpeedStepMillis = nowMillis + _speedStepMs;
            }
            float peakBoost = 1.0f;
            if (_speedFraction > 0.8f) {
                peakBoost = 1.0f + (_peakScale - 1.0f) * ((_speedFraction - 0.8f) / 0.2f);
            }
            _nextMove.rpm = _baseRpm * _speedFraction * peakBoost;
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _dirMinMs = 2000, _dirMaxMs = 9000;
        unsigned long _speedStepMs = 600;
        unsigned long _nextDirEventMillis = 0;
        unsigned long _nextSpeedStepMillis = 0;
        float _minFraction = 0.2f;
        float _maxStepFraction = 0.25f;
        float _speedFraction = 0.6f;
        bool _reverse = false;
};

/**************************************************************************/
/*!
  @brief  Direction flips on a timer like Metronome Flip, but the hold
  time between flips shortens progressively over a cycle - starts with
  long, deliberate holds in each direction and escalates toward rapid
  flips before resetting back to the slow start. An "escalation" shape
  for reversal RATE instead of speed (contrast RampingEscalation, which
  escalates speed with constant direction). Self-contained, no stroke
  telemetry.

  BENCH-VERIFY BEFORE RELYING ON THIS ONE: near the top of the
  escalation, the requested flip interval can approach the engine's
  250ms floor (setMinReversalIntervalMs()) - MIN_SAFE_HOLD_MS below
  keeps this pattern from ever requesting faster than that on its own,
  but if you've changed the engine's floor, keep MIN_SAFE_HOLD_MS >= it,
  or this pattern will just get silently rate-limited by the engine
  instead of behaving as designed. Feel out end_holdMs/escalationMillis
  on the bench rather than trusting the defaults blind - this is exactly
  the kind of pattern the project's motor-safety notes warn shouldn't
  assume the floor will save it.
*/
/**************************************************************************/
class EscalatingReversal : public RotationPattern {
    public:
        EscalatingReversal(const char *str) : RotationPattern(str) {}

        //! @param startHoldMs      hold time per direction at the start of a cycle (slow)
        //! @param endHoldMs        hold time per direction at the end of a cycle (fast) -
        //!                          clamped up to MIN_SAFE_HOLD_MS, see class comment
        //! @param escalationMillis total time to go from startHoldMs to endHoldMs before reset
        void setEscalationParameters(unsigned long startHoldMs, unsigned long endHoldMs,
                                      unsigned long escalationMillis) {
            _startHoldMs = startHoldMs;
            _endHoldMs = max(endHoldMs, (unsigned long)MIN_SAFE_HOLD_MS);
            _escalationMillis = escalationMillis;
        }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _reverse = _baseDirection;
            _cycleStartMillis = nowMillis;
            _nextFlipMillis = nowMillis + _startHoldMs;
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            unsigned long elapsedInCycle = nowMillis - _cycleStartMillis;
            if (elapsedInCycle >= _escalationMillis) {
                _cycleStartMillis = nowMillis;
                elapsedInCycle = 0;
            }
            if (nowMillis >= _nextFlipMillis) {
                _reverse = !_reverse;
                float escalationFraction = (float)elapsedInCycle / (float)_escalationMillis;
                unsigned long currentHoldMs = _startHoldMs -
                    (unsigned long)((float)(_startHoldMs - _endHoldMs) * escalationFraction);
                _nextFlipMillis = nowMillis + max(currentHoldMs, (unsigned long)MIN_SAFE_HOLD_MS);
            }
            _nextMove.rpm = _baseRpm;
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        // Floor this pattern will not request a flip faster than, independent
        // of (but intended to stay >=) RotationEngine::setMinReversalIntervalMs().
        // See class-level bench-verify note above.
        static const unsigned long MIN_SAFE_HOLD_MS = 300;
        unsigned long _startHoldMs = 3000;
        unsigned long _endHoldMs = 500;
        unsigned long _escalationMillis = 20000;
        unsigned long _cycleStartMillis = 0;
        unsigned long _nextFlipMillis = 0;
        bool _reverse = false;
};

/**************************************************************************/
/*!
  @brief  The full "climax" cycle, choreographed with the stroke axis's
  paired "Depth Pulse" pattern (DaddyBearRyte_StrokePattern_DepthPulse.h):

    1. Normal push-in: spins at the knob-controlled Speed (_baseRpm),
       same as Steady - no different from any other pattern while the
       stroke axis is on its way to full depth.
    2. The instant the stroke axis reaches full depth (a rising-edge
       crossing of _triggerThreshold, same detection as Depth Surge/
       Threshold Burst): opens a BURST WINDOW. Speed jumps to the
       engine's TRUE maximum (_maxRpmCeiling, fed via setMaxRpmCeiling()
       - see the base class) - this deliberately IGNORES the Speed knob
       for the duration of the window, per the design brief. Direction
       then flips repeatedly at even intervals for as long as the
       window is open (see "window length" and "flip interval" below).
    3. The window itself lasts exactly as long as Depth Pulse's own
       vibration hold - both patterns compute an identical duration from
       the same Sensation value, using the same formula and (by default)
       the same [_minDurationSeconds, _maxDurationSeconds] range - so
       "start vibrating" and "start the speed/direction burst" begin and
       end together without any direct messaging between the two
       patterns, just a shared cross-axis input.
    4. When the window closes, speed reverts to the knob-controlled
       _baseRpm (simple rotation again) - direction is left wherever the
       last flip landed it (not reset to the manual Reverse checkbox;
       see the note below). This is also naturally when the stroke axis
       is pulling back out, since both windows are sized identically.
    5. Repeats every time the stroke axis next arrives at full depth.

  WITHIN the window, direction flips at an interval that is a
  PERCENTAGE of the window's own length (setReversalIntervalPercentage(),
  default 50%) - e.g. a 5-second window at 50% flips every 2.5 seconds
  (twice within the window); at 25%, every 1.25 seconds (four times).
  This percentage is a firmware setting, not a live control - there's no
  second Sensation-style knob on the rotation screen, so "the rotation
  pattern's own sensation setting" the design brief refers to is this
  configured percentage, applied to the Sensation-derived window length
  both patterns already share.

  A trigger arriving while a window is already open is ignored (doesn't
  restart or extend it) - same "don't re-trigger mid-event" rule as the
  rest of this file's trigger-based patterns.

  Design assumption worth flagging: direction is NOT reset to the manual
  Reverse checkbox when the window closes - it just holds wherever the
  last flip left it, consistent with every other direction-holding
  pattern in this file. If the intent was for withdrawal to always
  resume in a specific (checkbox) direction rather than whatever the
  burst last landed on, that's a small follow-up change.

  Ignores setBaseDirection() as an ONGOING input the same way Alternating
  Sweep/Metronome Flip do - the checkbox only seeds the starting
  direction at reset().

  Needs THREE feeds: RotationEngine::setSyncedStrokePosition() (depth
  trigger), RotationEngine::setSyncedStrokeSensation() (window length),
  and RotationEngine::setMaxRpm() having been called at least once so
  _maxRpmCeiling (inherited from the base class) is non-zero - otherwise
  the burst phase will spin at 0 RPM instead of true max. Also depends
  on the same position-range calibration placeholder as Depth Surge/
  Threshold Burst - see PATTERN-INTEGRATION-NOTES.md section 3.
*/
/**************************************************************************/
class DepthTriggerSpin : public RotationPattern {
    public:
        DepthTriggerSpin(const char *str) : RotationPattern(str) {}

        //! Stroke-position fraction (0-1) that counts as "reached full
        //! depth" and opens a burst window. Default 0.95 - just shy of
        //! 1.0 so it reliably fires even if the stroke axis doesn't hit
        //! exactly 1.0 every cycle.
        void setTriggerThreshold(float thresholdFraction) { _triggerThreshold = constrain(thresholdFraction, 0.0f, 1.0f); }

        //! Sensation-to-seconds mapping range for the burst WINDOW's
        //! total length - matches Depth Pulse's setDurationRange() by
        //! default (1.0s-30.0s) so the two patterns' windows stay
        //! identical in length by default. -100 = shortest, +100 =
        //! longest.
        void setDurationRange(float minSeconds, float maxSeconds) {
            _minDurationSeconds = minSeconds;
            _maxDurationSeconds = maxSeconds;
        }

        //! How often direction flips WITHIN the window, as a percentage
        //! of the window's own Sensation-derived length. Default 50.
        void setReversalIntervalPercentage(float percent) { _reversalIntervalPercentage = max(0.01f, percent); }

        //! Safety floor on the computed flip interval (window length x
        //! percentage above), regardless of how the duration range /
        //! percentage are configured. Default 500ms - comfortably above
        //! RotationEngine's own global 250ms setMinReversalIntervalMs()
        //! backstop. Not a spec pulled from the motor/driver datasheet
        //! (not in hand) - bench-verify before relying on intervals
        //! close to it.
        void setMinimumIntervalMs(unsigned long ms) { _minimumIntervalMs = ms; }

        //! Live mode only: the burst ends when the stroke falls below this
        //! fraction. Lower than the trigger so Depth Pulse's vibration at
        //! full depth does not chop the burst.
        void setExitThreshold(float thresholdFraction) { _exitThreshold = constrain(thresholdFraction, 0.0f, 1.0f); }

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _reverse = _baseDirection; // seed from manual choice; not re-read after this
            _bursting = false;
            _latched = false;
            _windowEndMillis = 0;
            _nextFlipMillis = 0;
            _flipIntervalMs = 0;
            _lastPositionFraction = 0.0f;
            _havePosition = false;
        }

        //! Rotation Sensation scales the flip spacing during the burst:
        //! negative = faster flips, positive = slower, 0 = the default
        //! spacing (percentage of the stroke-Sensation window, 500ms floor).
        //! It does not change the burst speed or the burst length.
        //!
        //! Two modes:
        //!  - LIVE (a live stroke position is being fed): the burst starts
        //!    when the stroke reaches full depth and holds, flipping
        //!    direction, for as long as the stroke stays at depth, then
        //!    drops back to the knob speed. A safety cap (1.5x the longest
        //!    window) ends a burst if the stroke never leaves; the pattern
        //!    then waits for the stroke to leave depth before re-arming.
        //!  - FIXED WINDOW (no live position): the original behavior, a
        //!    burst window sized by the stroke Sensation slider.
        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            float spacingScale = _sensScale();

            if (_haveLivePosition) {
                bool atDepth = _bursting
                    ? (_strokeLiveFraction >= _exitThreshold)
                    : (_strokeLiveFraction >= _triggerThreshold);
                if (!atDepth) {
                    _bursting = false;
                    _latched = false;   // left full depth: re-arm
                } else if (!_bursting && !_latched) {
                    _bursting = true;
                    _windowEndMillis = nowMillis + (unsigned long)(_maxDurationSeconds * 1500.0f);
                    _flipIntervalMs = _flipSpacingMs(_windowMs(), spacingScale);
                    _nextFlipMillis = nowMillis + _flipIntervalMs;
                }
                if (_bursting && nowMillis >= _windowEndMillis) {
                    _bursting = false;
                    _latched = true;    // safety cap hit: wait for the stroke to leave depth
                }
                if (_bursting && nowMillis >= _nextFlipMillis) {
                    _reverse = !_reverse;
                    _flipIntervalMs = _flipSpacingMs(_windowMs(), spacingScale);
                    _nextFlipMillis += _flipIntervalMs;
                }
            } else {
                bool crossedThreshold = _havePosition
                    && (_strokePositionFraction >= _triggerThreshold)
                    && (_lastPositionFraction < _triggerThreshold);
                _lastPositionFraction = _strokePositionFraction;
                _havePosition = true;

                if (crossedThreshold && !_bursting) {
                    unsigned long windowMs = _windowMs();
                    _bursting = true;
                    _windowEndMillis = nowMillis + windowMs;
                    _flipIntervalMs = _flipSpacingMs(windowMs, spacingScale);
                    _nextFlipMillis = nowMillis + _flipIntervalMs;
                }
                if (_bursting) {
                    if (nowMillis >= _windowEndMillis) {
                        _bursting = false;
                    } else if (nowMillis >= _nextFlipMillis) {
                        _reverse = !_reverse;
                        _nextFlipMillis += _flipIntervalMs; // scheduled off the last flip, not "now" - avoids drift
                    }
                }
            }

            _nextMove.rpm = _bursting ? (_maxRpmCeiling * _peakScale) : _baseRpm;
            _nextMove.reverse = _reverse;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        unsigned long _windowMs() const {
            float windowSeconds = _minDurationSeconds
                + (_strokeSensationFraction + 1.0f) * 0.5f * (_maxDurationSeconds - _minDurationSeconds);
            return (unsigned long)(windowSeconds * 1000.0f);
        }
        unsigned long _flipSpacingMs(unsigned long windowMs, float scale) const {
            unsigned long ms = (unsigned long)((float)windowMs * (_reversalIntervalPercentage / 100.0f) * scale);
            return (ms < _minimumIntervalMs) ? _minimumIntervalMs : ms;
        }

        float _triggerThreshold = 0.95f;
        float _exitThreshold = 0.75f;
        bool _latched = false;
        float _minDurationSeconds = 1.0f;
        float _maxDurationSeconds = 30.0f;
        float _reversalIntervalPercentage = 50.0f;
        unsigned long _minimumIntervalMs = 500;
        bool _bursting = false;
        unsigned long _windowEndMillis = 0;
        unsigned long _nextFlipMillis = 0;
        unsigned long _flipIntervalMs = 0;
        bool _reverse = false;
        float _lastPositionFraction = 0.0f;
        bool _havePosition = false;
};

/**************************************************************************/
/*
  Array holding all rotation patterns. Add any new pattern here - this is
  what the M5 remote's rotation pattern selector indexes into, same as
  StrokeEngine's patternTable[] for the stroke side.

  ORDER IS LOAD-BEARING - see PATTERN-INTEGRATION-NOTES.md "cross-file
  sync": the M5 remote's ui_RotationPatternS roller option string must
  list these same 16 names in this same order, or the selector and the
  firmware disagree about which index means which pattern.
*/
/**************************************************************************/
static RotationPattern *rotationPatternTable[] = {
  new SteadySpeed("Steady"),
  new OscillatingSweep("Sweep"),
  new RampingEscalation("Escalation"),
  new RandomizedBursts("Random Bursts"),
  new ScrewSync("Screw Sync"),
  new StutterSync("Stutter Sync"),
  new BottomHoldSync("Bottom Hold Sync"),
  new DepthSurge("Depth Surge"),
  new ThresholdBurst("Threshold Burst"),
  new TempoMatch("Tempo Match"),
  new SnapReverse("Snap Reverse"),
  new AlternatingSweep("Alternating Sweep"),
  new MetronomeFlip("Metronome Flip"),
  new DrunkenWalk("Drunken Walk"),
  new EscalatingReversal("Escalating Reversal"),
  new DepthTriggerSpin("Depth Trigger Spin")
  // <-- insert your new rotation pattern here!
};

static const unsigned int rotationPatternTableSize = sizeof(rotationPatternTable) / sizeof(rotationPatternTable[0]);
