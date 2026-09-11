/**
 *   Depth Pulse - custom StrokeEngine pattern for the Daddy Bear Ryte
 *   CyborgTop OSSM build.
 *
 *   NOT part of the upstream ortlof/OSSM-Stroke repo - this is a
 *   standalone addition. Insert this class into
 *   lib/StrokeEngine/src/pattern.h (anywhere among the other pattern
 *   classes - order doesn't matter for the class definitions themselves,
 *   only for patternTable[] below), then add one line to patternTable[]
 *   at the bottom of that file:
 *
 *       new DepthPulse("Depth Pulse"),
 *
 *   right before the "// <-- insert your new pattern class here!" comment
 *   (patternTable[] currently ends at index 8, StrokeNibbler - this
 *   becomes index 9).
 *
 *   Modeled directly on two real patterns in the same file: JackHammer's
 *   vibration technique (alternating small moves at max speed/
 *   acceleration) for HOW it vibrates, and StopNGo's delay-timer usage
 *   (_startDelay()/_isStillDelayed(), inherited from the Pattern base
 *   class) for HOW LONG - that's the real, proven mechanism this codebase
 *   already uses to create time-based holds inside an index-driven
 *   pattern (StrokeEngine calls nextTarget(index) once per completed
 *   move, not on a timer, so "hold for N seconds" has to be built this
 *   way rather than by just checking millis() directly against a target).
 *
 *   2026-09-10 revision: Sensation now controls how LONG the vibration
 *   hold lasts (in seconds) rather than how strong it is. Vibration
 *   amplitude is a fixed setting instead (setVibrationAmplitude()) - the
 *   inverse of Jack Hammer/Stroke Nibbler, where Sensation sets amplitude
 *   and duration isn't a concept that applies to them.
 *
 *   Built to pair with the rotation axis's "Depth Trigger Spin" pattern
 *   (RotationPattern.h) - that pattern derives its own burst-window
 *   length from the SAME Sensation value by default (see its
 *   setDurationRange()), so turning Sensation up lengthens both the
 *   vibration hold here and the rotation burst window there together.
 *   Either pattern works fine on its own without the other.
 */

/**************************************************************************/
/*!
  @brief  Strokes in smoothly to full depth (same trapezoidal profile as
  Simple Stroke - no vibration on the way in), then vibrates in place at
  full depth for a duration set by Sensation, then pulls all the way back
  out smoothly in one go. Sensation controls HOLD TIME (seconds), not
  amplitude - vibration amplitude is a separate, fixed setting.
*/
/**************************************************************************/
class DepthPulse : public Pattern {
    public:
        DepthPulse(const char *str) : Pattern(str) {}

        //! Sensation maps to vibration-hold duration in seconds, across
        //! [_minDurationSeconds, _maxDurationSeconds] - see
        //! setDurationRange(). -100 = shortest hold, +100 = longest.
        void setSensation(float sensation) {
            _sensation = sensation;
            float seconds = fmap(sensation, -100.0, 100.0, _minDurationSeconds, _maxDurationSeconds);
            _updateDelay((int)(seconds * 1000.0f));
        }

        void setTimeOfStroke(float speed = 0) { _timeOfStroke = speed; }
        void setStroke(int stroke) { _stroke = stroke; }
        void setSpeedLimit(unsigned int maxSpeed, unsigned int maxAcceleration, unsigned int stepsPerMM) {
            _maxSpeed = maxSpeed;
            _maxAcceleration = maxAcceleration;
            _stepsPerMM = stepsPerMM;
            _updateVibrationDistance();
        }

        //! Hold-duration range (seconds) that Sensation maps across.
        //! Default 1.0s (Sensation -100) to 30.0s (Sensation +100) - this
        //! is the "hang time" that Depth Trigger Spin also derives its own
        //! reversal timing from by default. Keep this matched to Depth
        //! Trigger Spin's setDurationRange() if you want the two axes'
        //! Sensation-driven timing to stay in lockstep (they default to
        //! the same range already).
        void setDurationRange(float minSeconds, float maxSeconds) {
            _minDurationSeconds = minSeconds;
            _maxDurationSeconds = maxSeconds;
            setSensation(_sensation); // re-apply current sensation against the new range
        }

        //! Fixed vibration amplitude in mm - no longer tied to Sensation
        //! (Sensation now controls hold duration instead, see above).
        //! Default 8mm, roughly the midpoint of Jack Hammer/Stroke
        //! Nibbler's old 1-15mm sensation-driven range.
        void setVibrationAmplitude(float mm) {
            _vibrationAmplitudeMm = mm;
            _updateVibrationDistance();
        }

        motionParameter nextTarget(unsigned int index) {

            // Fresh start, or we just completed a withdrawal: smooth move
            // IN to full depth. No vibration on this leg.
            if (index == 0 || _phase == PHASE_WITHDRAWN) {
                _nextMove.speed = int(1.5 * _stroke / _timeOfStroke);
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed) / _timeOfStroke);
                _nextMove.stroke = _depth;
                _phase = PHASE_STROKING_IN;
                _index = index;
                _nextMove.skip = false;
                return _nextMove;
            }

            // only calculate new position, if index has incremented - same
            // "vibration is sufficiently fast, no mid-stroke update needed"
            // reasoning JackHammer uses.
            if (index != _index) {
                if (_phase == PHASE_STROKING_IN) {
                    // The move above completed - we're at full depth now.
                    // Start the Sensation-sized hold timer.
                    _phase = PHASE_VIBRATING;
                    _startDelay();
                    _vibrationToggle = false;
                }

                if (_phase == PHASE_VIBRATING) {
                    if (_isStillDelayed() == false) {
                        // Timer's up - one smooth move all the way back out.
                        _nextMove.speed = int(1.5 * _stroke / _timeOfStroke);
                        _nextMove.acceleration = int(3.0 * float(_nextMove.speed) / _timeOfStroke);
                        _nextMove.stroke = _depth - _stroke;
                        _phase = PHASE_WITHDRAWN;
                    } else {
                        // Keep vibrating in place at max speed/acceleration
                        // until the timer expires. Clamped so it can never
                        // travel shallower than the normal stroke's own
                        // shallow end, even with a large amplitude setting.
                        _nextMove.speed = _maxSpeed;
                        _nextMove.acceleration = _maxAcceleration;
                        _vibrationToggle = !_vibrationToggle;
                        _nextMove.stroke = _vibrationToggle
                            ? max(_depth - _vibrationDistance, _depth - _stroke)
                            : _depth;
                    }
                }
            }
            _index = index;
            _nextMove.skip = false;
            return _nextMove;
        }

    protected:
        enum Phase { PHASE_STROKING_IN, PHASE_VIBRATING, PHASE_WITHDRAWN };
        Phase _phase = PHASE_WITHDRAWN;
        bool _vibrationToggle = false;
        float _minDurationSeconds = 1.0f;
        float _maxDurationSeconds = 30.0f;
        float _vibrationAmplitudeMm = 8.0f;
        int _vibrationDistance = 0;

        void _updateVibrationDistance() {
            _vibrationDistance = (int)(_vibrationAmplitudeMm * (float)_stepsPerMM);
        }
};
