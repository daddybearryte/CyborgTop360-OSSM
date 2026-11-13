/**
 *   Live tuning for the Burst Pull-out stroke pattern.
 *
 *   A tiny standalone header (no dependency on pattern.h or the Pattern class) so main.cpp can write the values
 *   without pulling a second copy of patternTable[] into its translation unit. The pattern reads them on every call.
 *
 *   The accessor holds a function-local static inside an inline function: C++11 guarantees every translation unit
 *   that includes this header gets the SAME object, which a plain static in a header would not.
 */

#pragma once

struct BurstPullOutTuning {
    volatile int   count     = 5;      //!< fast strokes (in+out) per run, the last one ends in the slow pull-out. 1 to 12.
    volatile float pullScale = 1.0f;   //!< scales the whole pull-out slowdown range together. 0.5 to 2.0, 1.0 = 2x to 10x.
};

inline BurstPullOutTuning& burstPullOutTuning() {
    static BurstPullOutTuning tuning;
    return tuning;
}
