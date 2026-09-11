/**
 *   RotationEngine - drives the CyborgTop rotation axis through the
 *   Sanyoudian driver via the ULN2803A level-shifter on J9.
 *
 *   NOT a second StrokeEngine instance (see checkpoint doc, "Rotation
 *   Pattern Library" section) - rotation is continuous with no endstops,
 *   so instead of FastAccelStepper's trapezoidal position moves, this
 *   generates a continuous step-pulse train with ESP32 LEDC hardware PWM
 *   and lets frequency = speed. DIR is a static GPIO level. There is no
 *   position to track, so there is no homing and no "stroke" concept -
 *   RotationEngine only ever asks "how fast, which way, right now."
 *
 *   IMPORTANT - signal polarity through the ULN2803A:
 *   The ULN2803A's Darlington outputs are open-collector sinks: input
 *   HIGH -> that channel's output pulled LOW -> current flows PLS+/DIR+/
 *   ENA+ (tied to +5V) through the Sanyoudian opto -> PLS-/DIR-/ENA-
 *   (the ULN2803 output) -> opto energized. Input LOW -> output floats
 *   -> no current -> opto off. So from firmware's point of view, GPIO
 *   HIGH = "opto energized" for DIR and ENABLE. What "opto energized"
 *   means physically (which direction, enabled vs. disabled) depends on
 *   the Sanyoudian driver's own convention, which isn't in hand - that's
 *   exactly what the step-9 first-live-test bench check is for. Don't
 *   assume; confirm on the bench, then set invertDirection /
 *   enableActiveHigh below to match reality. The PULSE line itself needs
 *   no such flag - a symmetric pulse train works the same regardless of
 *   which edge the opto latches on.
 *
 *   LEDC frequency floor: at the default 10-bit resolution, low RPM
 *   values may fall below what the LEDC timer can hit cleanly at 10-bit
 *   depth. If the lowest speed setting stutters or won't turn, drop
 *   ledcResolutionBits (constructor arg) to 6-8 bits - lower resolution
 *   buys a lower usable frequency floor since duty resolution isn't
 *   what matters here (this is a fixed ~50% pulse train, not an analog
 *   dimmer).
 *
 *   ==========================================================================
 *   2026-09-10 update: added setSyncedStrokePosition()/setSyncedStrokeSpeed()
 *   to feed the new position/speed-linked rotation patterns (Depth Surge,
 *   Threshold Burst, Tempo Match - see RotationPattern.h). Both take RAW
 *   values in whatever units StrokeEngine's telemetry callback reports
 *   (position, speed - see MAINBOARD-ROTATION-INTEGRATION.md section 4) and
 *   normalize them to a 0-1 fraction internally before forwarding to the
 *   active pattern, using the ranges configured below.
 *
 *   THESE RANGES ARE PLACEHOLDERS. Neither this project nor the prior
 *   hardware/integration work established StrokeEngine's actual position
 *   units (likely mm, keyed to Stroker.physicalTravel / the DEPTH-STROKE..
 *   DEPTH config window) or speed units (likely mm/s or a StrokeEngine-
 *   internal unit, not confirmed). Confirm both against the actual
 *   Stroker configuration/telemetry before trusting Depth Surge, Threshold
 *   Burst, or Tempo Match - see PATTERN-INTEGRATION-NOTES.md for what to
 *   check and how to calibrate. Until confirmed, treat these three
 *   patterns as untested rather than bench-verified like the rest of the
 *   axis.
 *   ==========================================================================
 *
 *   ==========================================================================
 *   2026-09-10 update #2: added setSyncedStrokeSensation() to feed the new
 *   Depth Trigger Spin rotation pattern (RotationPattern.h). Unlike the
 *   position/speed feeds above, Sensation's range is a fixed StrokeEngine
 *   spec (-100 to 100) - no bench calibration needed for the normalization
 *   itself, though Depth Trigger Spin's depth TRIGGER still depends on the
 *   same position-range calibration as Depth Surge/Threshold Burst.
 *   ==========================================================================
 *
 *   ==========================================================================
 *   2026-09-11 update: setMaxRpm() now also pushes the raw ceiling to
 *   every pattern via setMaxRpmCeiling() (RotationPattern.h base class) -
 *   needed by Depth Trigger Spin's redesigned burst phase, which
 *   deliberately spins at the machine's true max rather than the
 *   Speed-knob-scaled value every other pattern uses. Harmless for every
 *   pattern that doesn't use it.
 *   ==========================================================================
 */

#pragma once

#include <Arduino.h>
#include "RotationPattern.h"

class RotationEngine {

    public:
        //! Constructor
        /*!
          @param pulsePin          GPIO wired to J9 IO2 -> ULN2803 input 1 (PLS-)
          @param dirPin            GPIO wired to J9 IO15 -> ULN2803 input 2 (DIR-)
          @param enablePin         GPIO wired to J9 IO33 -> ULN2803 input 3 (ENA-)
          @param invertDirection   Flip if "forward" in firmware spins the wrong
                                    way on the bench - confirm at build step 9
          @param enableActiveHigh  true if driving enablePin HIGH is what
                                    energizes the driver - confirm at build step 9
          @param ledcChannel       LEDC channel to claim (stroke axis uses none
                                    of LEDC's 0-15 channels, so any is free -
                                    default 4 avoids clashing if future code
                                    claims low channel numbers first)
          @param ledcResolutionBits See "LEDC frequency floor" note above
        */
        RotationEngine(uint8_t pulsePin, uint8_t dirPin, uint8_t enablePin,
                        bool invertDirection = false, bool enableActiveHigh = true,
                        uint8_t ledcChannel = 4, uint8_t ledcResolutionBits = 10);

        //! Configure pins + LEDC. Leaves the driver disabled and pulses stopped -
        //! call enable()/startPattern() explicitly, same spirit as StrokeEngine
        //! requiring an explicit enableAndHome().
        void begin();

        //! Energize the driver (respects enableActiveHigh).
        void enable();

        //! De-energize the driver and stop the pulse train.
        void disable();

        //! Must match the Sanyoudian driver's DIP-switch pulses/rev setting
        //! (checkpoint doc: DIP switches set 400-40000 pulses/rev).
        void setPulsesPerRev(uint32_t ppr) { _pulsesPerRev = ppr; }

        //! Ceiling RPM - what 100% speed maps to. Also pushed to every
        //! pattern's setMaxRpmCeiling() (not just the active one) so a
        //! pattern that needs the true ceiling - bypassing the Speed
        //! knob, like Depth Trigger Spin's burst phase - always has a
        //! current value even if it's selected after this is called.
        void setMaxRpm(float rpm);

        //! Speed knob value from the M5 remote, 0-100. Converted to RPM against
        //! maxRpm and pushed into whichever pattern is currently active.
        void setSpeedPercent(float percent);

        //! The user's manual direction choice (M5 remote's "Reverse"
        //! checkbox on the rotation screen). Pushed into whichever pattern
        //! is active; patterns that manage their own direction state
        //! (ScrewSync and its variants, Alternating Sweep, Metronome Flip,
        //! etc. - see RotationPattern.h) only read this at pattern-select
        //! time, not every tick.
        void setBaseDirection(bool reverse);

        //! Select a pattern by index into rotationPatternTable. Resets the new
        //! pattern's phase so it starts clean rather than mid-cycle.
        void setPattern(uint8_t index);
        uint8_t getPattern() { return _patternIndex; }
        char *getPatternName(uint8_t index);

        //! Feed the stroke axis's current phase in, for patterns (ScrewSync
        //! and its variants) that synchronize to it. Call this from a
        //! StrokeEngine telemetry callback - see
        //! MAINBOARD-ROTATION-INTEGRATION.md. Cheap to call even when the
        //! active pattern doesn't use it.
        void setSyncedStrokeDirection(bool extending) { _strokeExtending = extending; }

        //! Feed the stroke axis's current RAW position in (same units
        //! StrokeEngine's telemetry callback reports - see class-level
        //! placeholder-range note above). Internally normalized to a 0-1
        //! fraction via the configured range before being forwarded to the
        //! active pattern as setStrokePosition(). Only Depth Surge and
        //! Threshold Burst act on it; cheap to call regardless.
        void setSyncedStrokePosition(float rawPosition);

        //! Configure the raw-position range that maps to the pattern-facing
        //! 0-1 fraction: minPosition -> 0.0 (fully retracted), maxPosition
        //! -> 1.0 (full depth). MUST match Stroker's actual configured
        //! travel window (its DEPTH-STROKE..DEPTH range, or equivalent) -
        //! the constructor defaults below are placeholders, not measured
        //! values. See PATTERN-INTEGRATION-NOTES.md.
        void setStrokePositionRange(float minPosition, float maxPosition) {
            _strokePositionMin = minPosition;
            _strokePositionMax = maxPosition;
        }

        //! Feed the stroke axis's current RAW speed in (same units
        //! StrokeEngine's telemetry callback reports - not confirmed, see
        //! class-level note above). Internally normalized to a 0-1 fraction
        //! via the configured reference max before being forwarded to the
        //! active pattern as setStrokeSpeed(). Only Tempo Match acts on it.
        void setSyncedStrokeSpeed(float rawSpeed);

        //! Feed the stroke axis's current Sensation slider value in (-100
        //! to 100 - a fixed StrokeEngine spec, unlike position/speed above
        //! this needs no bench calibration). Internally normalized to a
        //! -1.0 to 1.0 fraction before being forwarded to the active
        //! pattern as setStrokeSensation(). Only Depth Trigger Spin acts
        //! on it today.
        void setSyncedStrokeSensation(float rawSensation);

        //! Reference speed that maps to a 1.0 fraction (i.e. "as fast as
        //! Tempo Match will ever ask the rotation axis to spin, driven by
        //! stroke speed alone"). This is a tunable feel parameter, not a
        //! measured physical ceiling - raw speeds above this just clamp to
        //! 1.0 rather than being an error. Expect to adjust at the bench.
        void setStrokeSpeedReference(float maxExpectedSpeed) {
            _strokeSpeedReference = (maxExpectedSpeed > 0.0f) ? maxExpectedSpeed : 1.0f;
        }

        //! Whether the currently-active STROKE pattern is safe to phase-sync
        //! against - false while a vibration-heavy stroke pattern (Jack
        //! Hammer, Stroke Nibbler) is running. Call this whenever the stroke
        //! pattern changes (see main.cpp's isVibrationHeavyStrokePattern());
        //! forwarded to whichever rotation pattern is active every tick.
        //! Overridden to always-true if setSyncSafetyOverrideEnabled(false)
        //! has been called.
        void setStrokePatternSafeForSync(bool safe) { _strokeSyncSafe = safe; }

        //! Master on/off for the sync-safety guard above. Defaults to
        //! enabled (true). Set false to let sync-based patterns chase the
        //! stroke axis's direction unconditionally, including during
        //! vibration stroke patterns - only do this if you've independently
        //! satisfied yourself the motor/driver can take it; this removes a
        //! guard that exists specifically because that wasn't a given.
        void setSyncSafetyOverrideEnabled(bool enabled) { _syncSafetyOverrideEnabled = enabled; }

        //! Hard floor on how often the rotation axis will actually reverse
        //! direction, independent of pattern or the setting above - if a
        //! reversal is requested sooner than this after the last one, it's
        //! deferred (direction holds) until the interval has elapsed. This
        //! is a backstop against any pattern, current or future, asking for
        //! reversals faster than the motor/driver should see - not a
        //! substitute for pattern-level care, which is what actually
        //! produces sensible behavior (a held direction, or a deliberately
        //! paced escalation) rather than just a slower thrash. Default
        //! 250ms is a starting point, not a spec pulled from the Sanyoudian
        //! datasheet (not in hand) - tune it down only after confirming on
        //! the bench it's not stressing the driver/motor at the shorter
        //! interval.
        void setMinReversalIntervalMs(uint32_t ms) { _minReversalIntervalMs = ms; }

        //! Passthrough to ScrewSync::setInverted() (rotationPatternTable[4]).
        //! Flip once at bring-up if "screw in" turns out to unscrew instead -
        //! see MAINBOARD-ROTATION-INTEGRATION.md section 4c. A no-op if
        //! ScrewSync isn't in the table (defensive only; it always is as
        //! shipped in RotationPattern.h).
        void setScrewSyncInverted(bool inverted);

        //! Enable the driver and begin running the active pattern.
        void startPattern();

        //! Stop the pulse train. Leaves the driver enabled unless disable() is
        //! also called - mirrors Stroker.stopMotion() vs. Stroker.disable().
        void stopPattern();

        bool isRunning() { return _running; }

        //! Call every tick from rotationEngineTask (RotationEngine.cpp) - not
        //! meant to be called directly from loop() or another task.
        void update();

    private:
        uint8_t _pulsePin, _dirPin, _enablePin;
        bool _invertDirection;
        bool _enableActiveHigh;
        uint8_t _ledcChannel;
        uint8_t _ledcResolutionBits;

        uint32_t _pulsesPerRev = 1000;
        float _maxRpm = 200.0f;
        float _speedPercent = 0.0f;
        bool _baseDirection = false;
        uint8_t _patternIndex = 0;
        bool _running = false;
        bool _lastDirWritten = false; // tracks last DIR pin state so we only write on change
        unsigned long _lastDirChangeMillis = 0; // for the min-reversal-interval backstop
        bool _strokeExtending = false; // last value fed via setSyncedStrokeDirection()
        bool _strokeSyncSafe = true;   // last value fed via setStrokePatternSafeForSync()
        bool _syncSafetyOverrideEnabled = true;
        uint32_t _minReversalIntervalMs = 250;

        // Position/speed sync - raw values + normalization ranges. See the
        // class-level placeholder-range note at the top of this file.
        float _strokePositionMin = 0.0f;
        float _strokePositionMax = 150.0f;   // PLACEHOLDER - confirm against Stroker's actual travel (mm assumed)
        float _strokeSpeedReference = 100.0f; // PLACEHOLDER - tunable feel parameter, not measured

        void _applyTarget(const rotationMotionParameter &target);
        uint32_t _rpmToHz(float rpm);
};

//! FreeRTOS task wrapper. pvParameters must be a RotationEngine*.
//! Pinned/created in main.cpp's setup() alongside the other OSSM tasks.
void rotationEngineTask(void *pvParameters);
