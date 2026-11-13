/**
 *   Burst Pull-out - custom StrokeEngine pattern for the Daddy Bear Ryte CyborgTop OSSM build.
 *
 *   Included from pattern.h, after the Pattern base class and before patternTable[]. Needs Pattern, motionParameter
 *   and DEBUG_PATTERN from pattern.h, so do not include it anywhere else.
 */

#pragma once

#include "BurstTuning.h"

/**************************************************************************/
/*!
  @brief  Burst, then pull out. A run of fast full-length strokes (the
  burst), followed by one slow pull-out, then the run starts again.

  The Speed dial sets the burst: the in and out legs of the burst strokes
  run at the normal Simple Stroke profile for the dialed speed. The pull-out
  is the out leg of the LAST stroke in the run, stretched to take longer.

  Sensation sets how much slower: at -100 the pull-out takes 2x as long as a
  burst out-leg, at +100 it takes 10x as long (geometric in between, so the
  middle of the slider is about 4.5x). The Tweak screen's Pull-out setting
  scales that whole range together: 50% gives 1x to 5x, 200% gives 4x to 20x.
  The Tweak screen's Count setting is the number of fast strokes per run.
  Both live in burstPullOutTuning() (BurstTuning.h) and are read on every call.

  Stateless: the leg is derived from the stroke index alone, so there is no
  timer to drift and a skipped call cannot desync it. The StrokeEngine
  restarts the index at 0 whenever the pattern starts or is changed. If the
  count is changed mid-run the cycle simply re-aligns on the next index.

  Reversal note for the rotation axis: the burst legs reverse exactly as
  often as Simple Stroke at the same dial speed, so this pattern is NOT in
  main.cpp's vibration-heavy list and Screw Sync follows it normally.
*/
/**************************************************************************/
class BurstPullOut : public Pattern {
    public:
        BurstPullOut(const char *str) : Pattern(str) {}

        void setTimeOfStroke(float speed = 0) {
            // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed;
        }

        //! Sensation sets how much slower the pull-out is than a burst out-leg.
        void setSensation(float sensation) { _sensation = sensation; }

        motionParameter nextTarget(unsigned int index) {
            int burstCount = burstPullOutTuning().count;
            if (burstCount < 1) burstCount = 1;
            unsigned int legsPerRun = 2 * (unsigned int)burstCount;
            bool movingOut = (index % 2) != 0;
            bool isPullOut = movingOut && ((index % legsPerRun) == (legsPerRun - 1));

            // Time of this leg: a normal half-stroke, or the stretched pull-out.
            float factor = isPullOut ? _pullOutFactor() : 1.0f;
            float legTime = _timeOfStroke * factor;

            // maximum speed of the trapezoidal motion
            _nextMove.speed = int(1.5 * _stroke / legTime);

            // acceleration to meet the profile (1/3 - 1/3 - 1/3)
            _nextMove.acceleration = int(3.0 * float(_nextMove.speed) / legTime);

            _nextMove.stroke = movingOut ? (_depth - _stroke) : _depth;
            _nextMove.skip = false;

#ifdef DEBUG_PATTERN
            if (isPullOut) {
                Serial.println("Burst pull-out, x" + String(factor, 2) + " slower, run of " + String(burstCount));
            }
#endif

            _index = index;
            return _nextMove;
        }

    protected:
        // Base range of the pull-out slowdown (time multiple of a burst out-leg) at Sensation -100 and +100.
        static constexpr float BASE_MIN_FACTOR = 2.0f;
        static constexpr float BASE_MAX_FACTOR = 10.0f;

        // Slowdown for the current Sensation and Pull-out scale, geometric between the scaled min and max.
        float _pullOutFactor() const {
            float scale = burstPullOutTuning().pullScale;
            float minFactor = BASE_MIN_FACTOR * scale;
            if (minFactor < 1.0f) minFactor = 1.0f;
            float maxFactor = BASE_MAX_FACTOR * scale;
            if (maxFactor < minFactor) maxFactor = minFactor;
            float t = (constrain(_sensation, -100.0f, 100.0f) + 100.0f) / 200.0f;
            return minFactor * powf(maxFactor / minFactor, t);
        }
};
