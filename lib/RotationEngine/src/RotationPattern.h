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
        //! only ScrewSync currently acts on it.
        virtual void setStrokeSyncSafe(bool safe) { _strokeSyncSafe = safe; }

        //! The user's manually-chosen base direction (M5 remote's rotation
        //! screen "Reverse" checkbox), false = forward. Honored by every
        //! pattern except ScrewSync, whose direction is phase-derived
        //! instead - same "most patterns use it, one pattern has its own
        //! reason not to" shape as setStrokeSyncSafe() above.
        virtual void setBaseDirection(bool reverse) { _baseDirection = reverse; }

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
        bool _baseDirection = false;
        bool _strokeExtending = false;
        bool _strokeSyncSafe = true;
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
  window elapses. Direction stays constant.
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

        void reset(unsigned long nowMillis) {
            _phaseStartMillis = nowMillis;
            _bursting = false;
            _nextEventMillis = 0; // forces a fresh random window on the next tick
        }

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            if (nowMillis >= _nextEventMillis) {
                _bursting = !_bursting;
                unsigned long window = _bursting
                    ? random(_burstMinMs, _burstMaxMs)
                    : random(_restMinMs, _restMaxMs);
                _nextEventMillis = nowMillis + window;
            }
            _nextMove.rpm = _bursting ? _baseRpm : (_baseRpm * _restFraction);
            _nextMove.reverse = _baseDirection; // user's manual direction choice
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _bursting = false;
        unsigned long _nextEventMillis = 0;
        unsigned long _burstMinMs = 400, _burstMaxMs = 1500;
        unsigned long _restMinMs = 800, _restMaxMs = 3000;
        float _restFraction = 0.25f;
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

        rotationMotionParameter nextTarget(unsigned long nowMillis) {
            _nextMove.rpm = _baseRpm;
            if (_strokeSyncSafe) {
                _nextMove.reverse = _inverted ? _strokeExtending : !_strokeExtending;
            }
            // else: leave _nextMove.reverse exactly as it was - hold the
            // current direction rather than chase _strokeExtending. It's
            // being toggled faster than is safe to mirror (a vibration-
            // heavy stroke pattern is active, or safety was otherwise
            // withheld - see RotationEngine::setStrokePatternSafeForSync()).
            // This intentionally does NOT stop rotation, just freezes
            // direction until a stroke pattern flagged safe is selected
            // again.
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        bool _inverted = false;
};

/**************************************************************************/
/*
  Array holding all rotation patterns. Add any new pattern here - this is
  what the M5 remote's rotation pattern selector indexes into, same as
  StrokeEngine's patternTable[] for the stroke side.
*/
/**************************************************************************/
static RotationPattern *rotationPatternTable[] = {
  new SteadySpeed("Steady"),
  new OscillatingSweep("Sweep"),
  new RampingEscalation("Escalation"),
  new RandomizedBursts("Random Bursts"),
  new ScrewSync("Screw Sync")
  // <-- insert your new rotation pattern here!
};

static const unsigned int rotationPatternTableSize = sizeof(rotationPatternTable) / sizeof(rotationPatternTable[0]);
