#include "RotationEngine.h"
#include <string.h>

// Below this RPM, treat as "stopped" rather than asking LEDC for a
// near-zero frequency (which is either undefined or just noise at low
// pulses/rev settings).
#define ROTATION_MIN_RUNNABLE_RPM 0.5f

// Update tick rate. Fast enough that speed ramps/sweeps read as smooth,
// cheap enough to leave headroom for the espNowRemoteTask and CableRemoteTask
// this shares core 0 with (see xTaskCreatePinnedToCore call in main.cpp).
#define ROTATION_TICK_MS 30

// Direction-change dwell. When a reversal is carried out, the pulse train is
// held off for this long before the DIR edge and again after it, so the driver
// never sees a step pulse coincident with DIR changing (opto setup time) and
// the gearbox gets a brief unloaded moment at the turnaround. Blocks the
// rotation task for twice this value, only on an actual reversal.
#define ROTATION_REVERSAL_DWELL_MS 5

RotationEngine::RotationEngine(uint8_t pulsePin, uint8_t dirPin, uint8_t enablePin,
                                bool invertDirection, bool enableActiveHigh,
                                uint8_t ledcChannel, uint8_t ledcResolutionBits)
    : _pulsePin(pulsePin), _dirPin(dirPin), _enablePin(enablePin),
      _invertDirection(invertDirection), _enableActiveHigh(enableActiveHigh),
      _ledcChannel(ledcChannel), _ledcResolutionBits(ledcResolutionBits) {}

void RotationEngine::begin() {
    pinMode(_dirPin, OUTPUT);
    pinMode(_enablePin, OUTPUT);

    // Start disabled, direction "forward" (un-inverted default). Keep
    // _lastDirWritten in sync so the first update() tick doesn't redundantly
    // rewrite a pin already at the right level.
    _lastDirWritten = _invertDirection;
    _lastDirChangeMillis = millis(); // first real reversal still waits out the min interval
    digitalWrite(_dirPin, _lastDirWritten ? HIGH : LOW);
    digitalWrite(_enablePin, _enableActiveHigh ? LOW : HIGH); // disabled

    // Claim the LEDC channel on the pulse pin. Old-core (espressif32@3.5.0,
    // matching this project's platformio.ini) API: ledcSetup() configures
    // the timer, ledcAttachPin() routes it to a pin, ledcWrite() sets duty.
    // There's no separate "change frequency" call in this core - ledcSetup()
    // is called again on every speed change in _applyTarget(), which is
    // fine here since the rotation axis has no position to preserve across
    // a brief phase glitch (unlike the stroke axis).
    ledcSetup(_ledcChannel, 1000, _ledcResolutionBits); // placeholder freq, output is 0 duty (stopped) below
    ledcAttachPin(_pulsePin, _ledcChannel);
    ledcWrite(_ledcChannel, 0);

    for (unsigned int i = 0; i < rotationPatternTableSize; i++) {
        rotationPatternTable[i]->setBaseSpeed(0.0f);
    }
    _pushBurst();
}

void RotationEngine::enable() {
    digitalWrite(_enablePin, _enableActiveHigh ? HIGH : LOW);
}

void RotationEngine::disable() {
    _running = false;
    _currentRpm = 0.0f;
    _lastHz = 0;
    ledcWrite(_ledcChannel, 0);
    digitalWrite(_enablePin, _enableActiveHigh ? LOW : HIGH);
}

void RotationEngine::setMaxRpm(float rpm) {
    _maxRpm = rpm;
    _pushBurst();
}

void RotationEngine::setBurstPercent(float percent) {
    _burstPercent = constrain(percent, 0.0f, 100.0f);
    _pushBurst();
}

// Patterns with a burst phase read setMaxRpmCeiling() as "how fast to burst".
// That is the burst speed (cap x burst percent), never above the hard cap.
void RotationEngine::_pushBurst() {
    float burstRpm = _maxRpm * (_burstPercent / 100.0f);
    for (unsigned int i = 0; i < rotationPatternTableSize; i++) {
        rotationPatternTable[i]->setMaxRpmCeiling(burstRpm);
    }
}

void RotationEngine::setSpeedPercent(float percent) {
    percent = constrain(percent, 0.0f, 100.0f);
    _speedPercent = percent;
    float baseRpm = (_speedPercent / 100.0f) * _maxRpm;
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setBaseSpeed(baseRpm);
    }
}

void RotationEngine::setBaseDirection(bool reverse) {
    _baseDirection = reverse;
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setBaseDirection(_baseDirection);
    }
}

void RotationEngine::setPattern(uint8_t index) {
    if (index >= rotationPatternTableSize) return; // ignore out-of-range selection
    _patternIndex = index;
    float baseRpm = (_speedPercent / 100.0f) * _maxRpm;
    rotationPatternTable[_patternIndex]->setBaseSpeed(baseRpm);
    rotationPatternTable[_patternIndex]->setBaseDirection(_baseDirection);
    rotationPatternTable[_patternIndex]->reset(millis());
}

char *RotationEngine::getPatternName(uint8_t index) {
    if (index >= rotationPatternTableSize) return (char *)"";
    return rotationPatternTable[index]->getName();
}

void RotationEngine::setSyncedStrokePosition(float rawPosition) {
    float range = _strokePositionMax - _strokePositionMin;
    float fraction = (range != 0.0f)
        ? (rawPosition - _strokePositionMin) / range
        : 0.0f;
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setStrokePosition(fraction); // pattern-side constrain()s to 0-1
    }
}

void RotationEngine::setSyncedStrokeLivePosition(float rawPosition) {
    float range = _strokePositionMax - _strokePositionMin;
    float fraction = (range != 0.0f)
        ? (rawPosition - _strokePositionMin) / range
        : 0.0f;
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setStrokeLivePosition(fraction); // pattern-side constrain()s to 0-1
    }
}

void RotationEngine::setSyncedStrokeSpeed(float rawSpeed) {
    float fraction = (_strokeSpeedReference != 0.0f) ? (rawSpeed / _strokeSpeedReference) : 0.0f;
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setStrokeSpeed(fraction); // pattern-side constrain()s to 0-1
    }
}

void RotationEngine::setSyncedStrokeSensation(float rawSensation) {
    float fraction = constrain(rawSensation / 100.0f, -1.0f, 1.0f);
    if (_patternIndex < rotationPatternTableSize) {
        rotationPatternTable[_patternIndex]->setStrokeSensation(fraction);
    }
}

void RotationEngine::setRotationSensation(float percent) {
    _rotationSensationPercent = constrain(percent, -100.0f, 100.0f);
    float fraction = _rotationSensationPercent / 100.0f;
    for (unsigned int i = 0; i < rotationPatternTableSize; i++) {
        rotationPatternTable[i]->setRotationSensation(fraction);
    }
}

void RotationEngine::setScrewSyncInverted(bool inverted) {
    for (unsigned int i = 0; i < rotationPatternTableSize; i++) {
        if (strcmp(rotationPatternTable[i]->getName(), "Screw Sync") == 0) {
            // Arduino builds are typically compiled with RTTI disabled, so
            // this leans on the name match (rather than dynamic_cast) to
            // know the static_cast below is safe.
            static_cast<ScrewSync *>(rotationPatternTable[i])->setInverted(inverted);
            return;
        }
    }
}

void RotationEngine::startPattern() {
    rotationPatternTable[_patternIndex]->reset(millis());
    enable();
    _currentRpm = 0.0f;
    _lastApplyMillis = millis();
    _lastHz = 0;
    _running = true;
}

void RotationEngine::stopPattern() {
    _running = false;
    _currentRpm = 0.0f;
    _lastHz = 0;
    ledcWrite(_ledcChannel, 0);
}

uint32_t RotationEngine::_rpmToHz(float rpm) {
    if (rpm < 0) rpm = 0;
    return (uint32_t)((rpm / 60.0f) * _pulsesPerRev);
}

void RotationEngine::_applyTarget(const rotationMotionParameter &target) {
    if (target.skip) return; // pattern says: hold last output, nothing to do

    unsigned long now = millis();
    float rpm = target.rpm;
    if (rpm > _maxRpm) rpm = _maxRpm;   // HARD CAP: nothing, from any pattern, ever exceeds it

    // Direction is a static level. A reversal is only carried out once the
    // output has slowed to _reversalSafeRpm: until then the target is capped
    // at that speed, so the axis decelerates, flips, then ramps back up.
    // The min-interval floor still applies as the backstop on reversal rate.
    // The flip itself is wrapped in a pulse-off dwell (see
    // ROTATION_REVERSAL_DWELL_MS) so DIR never changes under a running pulse train.
    bool wantReverse = target.reverse != _invertDirection; // XOR against wiring inversion
    if (wantReverse != _lastDirWritten && (now - _lastDirChangeMillis >= _minReversalIntervalMs)) {
        if (_currentRpm <= _reversalSafeRpm + 0.01f) {
            ledcWrite(_ledcChannel, 0);          // pulses off
            _lastHz = 0;                         // force LEDC re-setup when pulses resume
            delay(ROTATION_REVERSAL_DWELL_MS);   // let the last pulse clear
            digitalWrite(_dirPin, wantReverse ? HIGH : LOW);
            delay(ROTATION_REVERSAL_DWELL_MS);   // DIR setup time before the next pulse
            _lastDirWritten = wantReverse;
            now = millis();
            _lastDirChangeMillis = now;
            _lastApplyMillis = now;              // dwell time must not count as slew time
        } else if (rpm > _reversalSafeRpm) {
            rpm = _reversalSafeRpm;   // slow down first, flip on a later tick
        }
    }

    // Slew limit toward the (possibly capped) target.
    unsigned long dtMs = now - _lastApplyMillis;
    _lastApplyMillis = now;
    if (dtMs > 100) dtMs = 100;   // after a pause, don't allow one huge jump
    if (_maxAccelRpmPerSec > 0.0f) {
        float step = _maxAccelRpmPerSec * ((float)dtMs / 1000.0f);
        if (rpm > _currentRpm + step) rpm = _currentRpm + step;
        else if (rpm < _currentRpm - step) rpm = _currentRpm - step;
    }
    _currentRpm = rpm;

    if (rpm < ROTATION_MIN_RUNNABLE_RPM) {
        ledcWrite(_ledcChannel, 0);
        _lastHz = 0;
        return;
    }

    uint32_t hz = _rpmToHz(rpm);
    if (hz != _lastHz) {
        uint32_t actualHz = ledcSetup(_ledcChannel, hz, _ledcResolutionBits);
        (void)actualHz; // available if you want to log the achieved-vs-requested frequency during bring-up
        _lastHz = hz;
    }
    ledcWrite(_ledcChannel, 1 << (_ledcResolutionBits - 1)); // ~50% duty - a symmetric pulse train
}

void RotationEngine::update() {
    if (!_running) return;
    if (_patternIndex >= rotationPatternTableSize) return;

    rotationPatternTable[_patternIndex]->setStrokeDirection(_strokeExtending);
    rotationPatternTable[_patternIndex]->setStrokeSyncSafe(_syncSafetyOverrideEnabled ? _strokeSyncSafe : true);
    if (_livePositionProvider != NULL) {
        setSyncedStrokeLivePosition(_livePositionProvider());   // actual stepper position, polled each tick
    }
    // Position/speed forwarding happens in setSyncedStrokePosition()/
    // setSyncedStrokeSpeed() at call time (driven by the telemetry
    // callback's own cadence, not this tick), not here - see those methods
    // above. Direction/sync-safety are re-pushed every tick because they're
    // engine-held state the pattern needs refreshed each cycle; position/
    // speed are event-driven from StrokeEngine instead.
    rotationMotionParameter target = rotationPatternTable[_patternIndex]->nextTarget(millis());
    _applyTarget(target);
}

void rotationEngineTask(void *pvParameters) {
    RotationEngine *engine = (RotationEngine *)pvParameters;
    const TickType_t tickInterval = ROTATION_TICK_MS / portTICK_PERIOD_MS;
    for (;;) {
        engine->update();
        vTaskDelay(tickInterval);
    }
}
