/**
 *   Patterns of the StrokeEngine
 *   A library to create a variety of stroking motions with a stepper or servo motor on an ESP32.
 *   https://github.com/theelims/StrokeEngine 
 *
 * Copyright (C) 2021 theelims <elims@gmx.net>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#pragma once

#include <Arduino.h>
#include <StrokeEngine.h>
#include <math.h>
#include "PatternMath.h"

#define DEBUG_PATTERN                 // Print some debug informations over Serial

#ifndef STRING_LEN
  #define STRING_LEN           64     // Bytes used to initialize char array. No path, topic, name, etc. should exceed this value
#endif

/**************************************************************************/
/*!
  @brief  struct to return all parameters FastAccelStepper needs to calculate
  the trapezoidal profile.
*/
/**************************************************************************/
typedef struct {
    int stroke;         //!< Absolute and properly constrainted target position of a move in steps 
    int speed;          //!< Speed of a move in Steps/second 
    int acceleration;   //!< Acceleration to get to speed or halt 
    bool skip;          //!< no valid stroke, skip this set an query for the next --> allows pauses between strokes
} motionParameter;


/**************************************************************************/
/*!
  @class Pattern 
  @brief  Base class to derive your pattern from. Offers a unified set of
          functions to store all relevant paramteres. These function can be
          overridenid necessary. Pattern should be self-containted and not 
          rely on any stepper/servo related properties. Internal book keeping
          is done in steps. The translation from real word units to steps is
          provided by the StrokeEngine. Also the sanity check whether motion
          parameters are physically possible are done by the StrokeEngine. 
          Imposible motion commands are clipped, cropped or adjusted while 
          still having a smooth appearance.  
*/
/**************************************************************************/
class Pattern {

    public:
        //! Constructor
        /*!
          @param str String containing the name of a pattern 
        */
        Pattern(const char *str) { strcpy(_name, str); }

        //! Set the time a normal stroke should take to complete
        /*! 
          @param speed time of a full stroke in [sec] 
        */
        virtual void setTimeOfStroke(float speed) { _timeOfStroke = speed; }

        //! Set the maximum stroke a pattern may have
        /*! 
          @param stroke stroke distance in Steps 
        */
        virtual void setStroke(int stroke) { _stroke = stroke; }

        //! Set the maximum depth a pattern may have
        /*! 
          @param stroke stroke distance in Steps 
        */
        virtual void setDepth(int depth) { _depth = depth; }

        //! Sensation is an additional parameter a pattern can take to alter its behaviour
        /*! 
          @param sensation Arbitrary value from -100 to 100, with 0 beeing neutral 
        */
        virtual void setSensation(float sensation) { _sensation = sensation; } 

        //! Retrives the name of a pattern
        /*! 
          @return c_string containing the name of a pattern 
        */
        char *getName() { return _name; }

        //! Calculate the position of the next stroke based on the various parameters
        /*! 
          @param index index of a stroke. Increments with every new stroke. 
          @return Set of motion parameteres like speed, acceleration & position
        */
        virtual motionParameter nextTarget(unsigned int index) {
            _index = index;
            return _nextMove;
        } 

        //! Communicates the maximum possible speed and acceleration limits of the machine to a pattern.
        /*! 
          @param maxSpeed maximum speed which is possible. Higher speeds get truncated inside StrokeEngine anyway.
          @param maxAcceleration maximum possible acceleration. Get also truncated, if impossible.
          @param stepsPerMM 
        */
        virtual void setSpeedLimit(unsigned int maxSpeed, unsigned int maxAcceleration, unsigned int stepsPerMM) { _maxSpeed = maxSpeed; _maxAcceleration = maxAcceleration; _stepsPerMM = stepsPerMM; } 

    protected:
        int _stroke;
        int _depth;
        float _timeOfStroke;
        float _sensation = 0.0;
        int _index = -1;
        char _name[STRING_LEN]; 
        motionParameter _nextMove = {0, 0, 0, false};
        int _startDelayMillis = 0;
        int _delayInMillis = 0;
        unsigned int _maxSpeed = 0;
        unsigned int _maxAcceleration = 0;
        unsigned int _stepsPerMM = 0;

        /*!
          @brief Start a delay timer which can be polled by calling _isStillDelayed(). 
          Uses internally the millis()-function.
        */
        void _startDelay() {
            _startDelayMillis = millis();
        } 

        /*! 
          @brief Update a delay timer which can be polled by calling _isStillDelayed(). 
          Uses internally the millis()-function.
          @param delayInMillis delay in milliseconds 
        */
        void _updateDelay(int delayInMillis) {
            _delayInMillis = delayInMillis;
        } 

        /*! 
          @brief Poll the state of a internal timer to create pauses between strokes. 
          Uses internally the millis()-function.
          @return True, if the timer is running, false if it is expired.
        */
        bool _isStillDelayed() {
            return (millis() > (_startDelayMillis + _delayInMillis)) ? false : true; 
        }

};

/**************************************************************************/
/*!
  @brief  Simple Stroke Pattern. It creates a trapezoidal stroke profile
  with 1/3 acceleration, 1/3 coasting, 1/3 deceleration. Sensation has 
  no effect.
*/
/**************************************************************************/
class SimpleStroke : public Pattern {
    public:
        SimpleStroke(const char *str) : Pattern(str) {}

        void setTimeOfStroke(float speed = 0) { 
             // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed; 
        }   

        motionParameter nextTarget(unsigned int index) {
            // maximum speed of the trapezoidal motion 
            _nextMove.speed = int(1.5 * _stroke/_timeOfStroke);

            // acceleration to meet the profile
            _nextMove.acceleration = int(3.0 * _nextMove.speed/_timeOfStroke);

            // odd stroke is moving out    
            if (index % 2) {
                _nextMove.stroke = _depth - _stroke;
            
            // even stroke is moving in
            } else {
                _nextMove.stroke = _depth;
            }

            _index = index;
            return _nextMove;
        }
};

/**************************************************************************/
/*!
  @brief  Simple pattern where the sensation value can change the speed 
  ratio between in and out. Sensation > 0 make the in move faster (up to 5x)
  giving a hard pounding sensation. Values < 0 make the out move going 
  faster. This gives a more pleasing sensation. The time for the overall 
  stroke remains the same. 
*/
/**************************************************************************/
class TeasingPounding : public Pattern {
    public:
        TeasingPounding(const char *str) : Pattern(str) {}
        void setSensation(float sensation) { 
            _sensation = sensation;
            _updateStrokeTiming();
        }
        void setTimeOfStroke(float speed = 0) {
            _timeOfStroke = speed;
            _updateStrokeTiming();
        }
        motionParameter nextTarget(unsigned int index) {
            // odd stroke is moving out
            if (index % 2) {
                // maximum speed of the trapezoidal motion
                _nextMove.speed = int(1.5 * _stroke/_timeOfOutStroke);

                // acceleration to meet the profile                  
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed)/_timeOfOutStroke);    
                _nextMove.stroke = _depth - _stroke;
            // even stroke is moving in
            } else {
                // maximum speed of the trapezoidal motion
                _nextMove.speed = int(1.5 * _stroke/_timeOfInStroke); 
     
                // acceleration to meet the profile            
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed)/_timeOfInStroke);    
                _nextMove.stroke = _depth;
            }
            _index = index;
            return _nextMove;
        }
    protected:
        float _timeOfFastStroke = 1.0;
        float _timeOfInStroke = 1.0;
        float _timeOfOutStroke = 1.0;
        void _updateStrokeTiming() {
            // calculate the time it takes to complete the faster stroke
            // Division by 2 because reference is a half stroke
            _timeOfFastStroke = (0.5 * _timeOfStroke) / fscale(0.0, 100.0, 1.0, 5.0, abs(_sensation), 0.0);
            // positive sensation, in is faster
            if (_sensation > 0.0) {
                _timeOfInStroke = _timeOfFastStroke;
                _timeOfOutStroke = _timeOfStroke - _timeOfFastStroke;
            // negative sensation, out is faster
            } else {
                _timeOfOutStroke = _timeOfFastStroke;
                _timeOfInStroke = _timeOfStroke - _timeOfFastStroke;
            }
#ifdef DEBUG_PATTERN
            Serial.println("TimeOfInStroke: " + String(_timeOfInStroke));
            Serial.println("TimeOfOutStroke: " + String(_timeOfOutStroke));
#endif
        }
};


/**************************************************************************/
/*!
  @brief  Robot Stroke Pattern. Sensation controls the acceleration of the
  stroke. Positive value increase acceleration until it is a constant speed
  motion (feels robotic). Neutral is equal to simple stroke (1/3, 1/3, 1/3).
  Negative reduces acceleration into a triangle profile.
*/
/**************************************************************************/ 
class RoboStroke : public Pattern {
    public:
        RoboStroke(const char *str) : Pattern(str) {}

        void setTimeOfStroke(float speed = 0) { 
             // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed; 
        }

        void setSensation(float sensation = 0) { 
            _sensation = sensation;
            // scale sensation into the range [0.05, 0.5] where 0 = 1/3
            if (sensation >= 0 ) {
              _x = fscale(0.0, 100.0, 1.0/3.0, 0.5, sensation, 0.0);
            } else {
              _x = fscale(0.0, 100.0, 1.0/3.0, 0.05, -sensation, 0.0);
            }
#ifdef DEBUG_PATTERN
            Serial.println("Sensation:" + String(sensation,0) + " --> " + String(_x,6));
#endif
        }

        motionParameter nextTarget(unsigned int index) {
            // maximum speed of the trapezoidal motion
            float speed = float(_stroke) / ((1 - _x) * _timeOfStroke);
            _nextMove.speed = int(speed); 

            // acceleration to meet the profile
            _nextMove.acceleration = int(speed / (_x * _timeOfStroke));

            // odd stroke is moving out    
            if (index % 2) {
                _nextMove.stroke = _depth - _stroke;
            
            // even stroke is moving in
            } else {
                _nextMove.stroke = _depth;
            }

            _index = index;
            return _nextMove;
        }
    protected:
        float _x = 1.0/3.0;
};

/**************************************************************************/
/*!
  @brief  Like Teasing or Pounding, but every second stroke is only half the
  depth. The sensation value can change the speed ratio between in and out. 
  Sensation > 0 make the in move faster (up to 5x) giving a hard pounding 
  sensation. Values < 0 make the out move going faster. This gives a more 
  pleasing sensation. The time for the overall stroke remains the same for
  all strokes, even half ones. 
*/
/**************************************************************************/
class HalfnHalf : public Pattern {
    public:
        HalfnHalf(const char *str) : Pattern(str) {}
        void setSensation(float sensation) { 
            _sensation = sensation;
            _updateStrokeTiming();
        }
        void setTimeOfStroke(float speed = 0) {
            _timeOfStroke = speed;
            _updateStrokeTiming();
        }
        motionParameter nextTarget(unsigned int index) {
            // check if this is the very first 
            if (index == 0) {
              //pattern started for the very fist time, so we start gentle with a half move
              _half = true;
            }

            // set-up the stroke length
            int stroke = _stroke;
            if (_half == true) {
                // half the stroke length
                stroke = _stroke / 2;
            } 

            // odd stroke is moving out
            if (index % 2) {
                // maximum speed of the trapezoidal motion
                _nextMove.speed = int(1.5 * stroke/_timeOfOutStroke);  

                // acceleration to meet the profile                  
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed)/_timeOfOutStroke);    
                _nextMove.stroke = _depth - _stroke;
                // every second move is half
                _half = !_half;
            // even stroke is moving in
            } else {
                // maximum speed of the trapezoidal motion
                _nextMove.speed = int(1.5 * stroke/_timeOfInStroke);  
     
                // acceleration to meet the profile            
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed)/_timeOfInStroke);    
                _nextMove.stroke = (_depth - _stroke) + stroke;  
            }
            _index = index;
            return _nextMove;
        }
    protected:
        float _timeOfFastStroke = 1.0;
        float _timeOfInStroke = 1.0;
        float _timeOfOutStroke = 1.0;
        bool _half = true;
        void _updateStrokeTiming() {
            // calculate the time it takes to complete the faster stroke
            // Division by 2 because reference is a half stroke
            _timeOfFastStroke = (0.5 * _timeOfStroke) / fscale(0.0, 100.0, 1.0, 5.0, abs(_sensation), 0.0);
            // positive sensation, in is faster
            if (_sensation > 0.0) {
                _timeOfInStroke = _timeOfFastStroke;
                _timeOfOutStroke = _timeOfStroke - _timeOfFastStroke;
            // negative sensation, out is faster
            } else {
                _timeOfOutStroke = _timeOfFastStroke;
                _timeOfInStroke = _timeOfStroke - _timeOfFastStroke;
            }
#ifdef DEBUG_PATTERN
            Serial.println("TimeOfInStroke: " + String(_timeOfInStroke));
            Serial.println("TimeOfOutStroke: " + String(_timeOfOutStroke));
#endif
        }
};

/**************************************************************************/
/*!
  @brief  The insertion depth ramps up gradually with each stroke until it
  reaches its maximum. It then resets and restars. Sensations controls how 
  many strokes there are in a ramp.
*/
/**************************************************************************/
class Deeper : public Pattern {
    public:
        Deeper(const char *str) : Pattern(str) {}

        void setTimeOfStroke(float speed = 0) { 
             // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed; 
        }   

        void setSensation(float sensation) { 
            _sensation = sensation;

            // maps sensation to useful values [2,22] with 12 beeing neutral
            if (sensation < 0) {
                _countStrokesForRamp = map(sensation, -100, 0, 2, 11);
            } else {
                _countStrokesForRamp = map(sensation, 0, 100, 11, 32);
            }
#ifdef DEBUG_PATTERN
            Serial.println("_countStrokesForRamp: " + String(_countStrokesForRamp));
#endif
        }

        motionParameter nextTarget(unsigned int index) {
            // How many steps is each stroke advancing         
            int slope = _stroke / (_countStrokesForRamp);

            // The pattern recycles so we use modulo to get a cycling index.
            // Factor 2 because index increments with each full stroke twice
            // add 1 because modulo = 0 is index = 1
            int cycleIndex = (index / 2) % _countStrokesForRamp + 1;

            // This might be not smooth, as the insertion depth may jump when 
            // sensation is adjusted.

            // Amplitude is slope * cycleIndex
            int amplitude = slope * cycleIndex;
#ifdef DEBUG_PATTERN
            Serial.println("amplitude: " + String(amplitude)
                         + " cycleIndex: " + String(cycleIndex));
#endif

            // maximum speed of the trapezoidal motion 
            _nextMove.speed = int(1.5 * amplitude/_timeOfStroke); 

            // acceleration to meet the profile
            _nextMove.acceleration = int(3.0 * _nextMove.speed/_timeOfStroke);

            // odd stroke is moving out    
            if (index % 2) {
                _nextMove.stroke = _depth - _stroke;
            
            // even stroke is moving in
            } else {
                _nextMove.stroke = (_depth - _stroke) + amplitude;
            }

            _index = index;
            return _nextMove;
        }
    
    protected:
        int _countStrokesForRamp = 2;

};

/**************************************************************************/
/*!
  @brief  Pauses between a series of strokes. 
  The number of strokes ramps from 1 stroke to 5 strokes and back. Sensation 
  changes the length of the pauses between stroke series.
*/
/**************************************************************************/
class StopNGo : public Pattern {
    public:
        StopNGo(const char *str) : Pattern(str) {}

        void setTimeOfStroke(float speed = 0) { 
             // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed; 
        }   

        void setSensation(float sensation) { 
            _sensation = sensation;

            // maps sensation to a delay from 100ms to 10 sec
            _updateDelay(map(sensation, -100, 100, 100, 10000));
        }

        motionParameter nextTarget(unsigned int index) {
            // maximum speed of the trapezoidal motion 
            _nextMove.speed = int(1.5 * _stroke/_timeOfStroke); 

            // acceleration to meet the profile
            _nextMove.acceleration = int(3.0 * _nextMove.speed/_timeOfStroke);

            // adds a delay between each stroke
            if (_isStillDelayed() == false) {

                // odd stroke is moving out    
                if (index % 2) {
                    _nextMove.stroke = _depth - _stroke;

                    if (_strokeIndex >= _strokeSeriesIndex) {
                        // Reset stroke index to 1
                        _strokeIndex = 0;

                        // change count direction once we reached the maximum number of strokes
                        if (_strokeSeriesIndex >= _numberOfStrokes) {
                            _countStrokesUp = false;
                        }

                        // change count direction once we reached one stroke counting down
                        if (_strokeSeriesIndex <= 1) {
                            _countStrokesUp = true;
                        }

                        // increment or decrement strokes counter
                        if (_countStrokesUp == true) {
                            _strokeSeriesIndex++;
                        } else {
                            _strokeSeriesIndex--;
                        }

                        // start delay after having moved out
                        _startDelay();
                    }
                    

                // even stroke is moving in
                } else {
                    _nextMove.stroke = _depth;
                    // Increment stroke index by one
                    _strokeIndex++;
                }
                _nextMove.skip = false;
            } else {
                _nextMove.skip = true;
            }

            _index = index;
            
            return _nextMove;
        }

    protected:
        int _numberOfStrokes = 5;
        int _strokeSeriesIndex = 1;
        int _strokeIndex = 0;
        bool _countStrokesUp = true;

};

/**************************************************************************/
/*!
  @brief  Sensation reduces the effective stroke length while keeping the
  stroke speed constant to the full stroke. This creates interesting 
  vibrational pattern at higher sensation values. With positive sensation the
  strokes will wander towards the front, with negative values towards the back.
*/
/**************************************************************************/
class Insist : public Pattern {
    public:
        Insist(const char *str) : Pattern(str) {}   

        void setSensation(float sensation) { 
            _sensation = sensation;

            // make invert sensation and make into a fraction of the stroke distance
            _strokeFraction = (100 - abs(sensation))/100.0f;

            _strokeInFront = (sensation > 0) ? true : false;

            _updateStrokeTiming();
        }

        void setTimeOfStroke(float speed = 0) { 
             // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed;
            _updateStrokeTiming();
        }   

        void setStroke(int stroke) {
            _stroke = stroke;
            _updateStrokeTiming();
        }

        motionParameter nextTarget(unsigned int index) {

            // acceleration & speed to meet the profile
            _nextMove.acceleration = _acceleration;
            _nextMove.speed = _speed;

            if (_strokeInFront) {
                // odd stroke is moving out
                if (index % 2) {
                    _nextMove.stroke = _depth - _realStroke;

                // even stroke is moving in
                } else {
                    _nextMove.stroke = _depth;  
                }

            } else {
                // odd stroke is moving out    
                if (index % 2) {
                    _nextMove.stroke = _depth - _stroke;
                    
                // even stroke is moving in
                } else {
                    _nextMove.stroke = (_depth - _stroke) + _realStroke;                
                }
            }

            _index = index;
            
            return _nextMove;
        }

    protected:
        int _speed = 0;
        int _acceleration = 0;
        int _realStroke = 0;
        float _strokeFraction = 1.0;
        bool _strokeInFront = false;
        void _updateStrokeTiming() {
            // maximum speed of the longest trapezoidal motion (full stroke)
            _speed = int(1.5 * _stroke/_timeOfStroke);

            // Acceleration to hold 1/3 profile with fractional strokes
            _acceleration = int(3.0 * _nextMove.speed/(_timeOfStroke * _strokeFraction));

            // Calculate fractional stroke length
            _realStroke = int((float)_stroke * _strokeFraction);
        }

};

/**************************************************************************/
/*!
  @brief  Vibrational pattern that works like a jack hammer. Vibrates on the 
  way in and pulls out smoothly in one go. Sensation sets the vibration 
  amplitude.
*/
/**************************************************************************/
class JackHammer : public Pattern {
    public:
        JackHammer(const char *str) : Pattern(str) {}
        void setSensation(float sensation) { 
            _sensation = sensation;
            _updateVibrationParameters();
        }
        void setTimeOfStroke(float speed = 0) {
            _timeOfStroke = 0.5 * speed;
            _updateVibrationParameters();
        }
        void setStroke(int stroke) { 
            _stroke = stroke;
            _updateVibrationParameters();
        }
        void setSpeedLimit(unsigned int maxSpeed, unsigned int maxAcceleration, unsigned int stepsPerMM) { 
            _maxSpeed = maxSpeed; 
            _maxAcceleration = maxAcceleration; 
            _stepsPerMM = stepsPerMM;
            _updateVibrationParameters(); 
        }
        motionParameter nextTarget(unsigned int index) {

            // revert position for the first move or if depth is exceeded
            if (index == 0 || _nextMove.stroke >= _depth) {
                // Return strokes goes at regular speed without vibration back to 0

                // maximum speed of the trapezoidal motion
                _nextMove.speed = int(1.5 * _stroke/_timeOfStroke);  
                // acceleration to meet the profile                  
                _nextMove.acceleration = int(3.0 * float(_nextMove.speed)/_timeOfStroke);  
                // all they way out to start
                _nextMove.stroke = _depth - _stroke;
                // we are done here
                return _nextMove;
            }

            // only calculate new position, if index has incremented: no mid-stroke update, as vibration is sufficiently fast
            // except for return stroke if depth is exceeded.
            if (index != _index) {
             
                // Vibration happens at maximum speed and acceleration of the machine
                _nextMove.speed = _maxSpeed;
                _nextMove.acceleration = _maxAcceleration;

                // odd stroke is shaking out
                if (index % 2) {  
                    _nextMove.stroke = _nextMove.stroke - _outVibrationDistance;
                // even stroke is shaking in
                } else {
                    // limit range to _depth
                    _nextMove.stroke = min((_nextMove.stroke + _inVibrationDistance), _depth);
                }
            }
            _index = index;
            return _nextMove;
        }
    protected:
        int _inVibrationDistance = 0;
        int _outVibrationDistance = 0;
        int _strokeInSpeed = 0;
        void _updateVibrationParameters() {
            // Hammering in takes considerable longer then backing off
            _strokeInSpeed = int(0.5 * _stroke/_timeOfStroke);

            // Scale vibration amplitude from 1mm to 15mm with sensation
            _inVibrationDistance = (int)fscale(-100.0, 100.0, (float)(3.0*_stepsPerMM), (float)(25.0*_stepsPerMM), _sensation, 0.0);

            /* Calculate _outVibrationDistance to match with stroking speed
               d_out = d_in * (v_vib - v_stroke) / (v_vib + v_stroke)
               Formula neglects acceleration. Real timing will be slower due to finite acceleration & deceleration
            */
           _outVibrationDistance = _inVibrationDistance * (_maxSpeed - _strokeInSpeed) / (_maxSpeed + _strokeInSpeed);

#ifdef DEBUG_PATTERN
            Serial.println("_maxSpeed: " + String(_maxSpeed) + " _strokeInSpeed: " + String(_strokeInSpeed)  + " _strokeOutSpeed: " + String(int(1.5 * _stroke/_timeOfStroke)));
            Serial.println("inDist: " + String(_inVibrationDistance) + " outDist: " + String(_outVibrationDistance));
#endif
            
        }
};

/**************************************************************************/
/*!
  @brief  Simple vibrational overlay pattern. Vibrates on the way in and out. 
  Sensation sets the vibration amplitude.
*/
/**************************************************************************/
class StrokeNibbler : public Pattern {
    public:
        StrokeNibbler(const char *str) : Pattern(str) {}
        void setSensation(float sensation) { 
            _sensation = sensation;
            _updateVibrationParameters();
        }
        void setTimeOfStroke(float speed = 0) {
            _timeOfStroke = speed;
            _updateVibrationParameters();
        }
        void setStroke(int stroke) { 
            _stroke = stroke;
            _updateVibrationParameters();
        }
        void setSpeedLimit(unsigned int maxSpeed, unsigned int maxAcceleration, unsigned int stepsPerMM) { 
            _maxSpeed = maxSpeed; 
            _maxAcceleration = maxAcceleration; 
            _stepsPerMM = stepsPerMM;
            _updateVibrationParameters(); 
        }
        motionParameter nextTarget(unsigned int index) {

            // revert position to start for the first stroke
            if (index == 0) {
                // Set motion parameter
                _nextMove.speed = int(1.5 * _stroke/_timeOfStroke);
                _nextMove.acceleration = _maxAcceleration;

                // go to back position
                _nextMove.stroke = _depth - _stroke;

                // store index and return
                _index = index;
                return _nextMove;
            }

            // Vibration happens at maximum speed and acceleration of the machine
            _nextMove.speed = _maxSpeed;
            _nextMove.acceleration = _maxAcceleration;

            // check if we have reached one of the ends and reverse direction
            if (_nextMove.stroke >= _depth) {
                _returnStroke = true;
            } 
            if (_nextMove.stroke <= (_depth - _stroke)) {
                _returnStroke = false;
            }

            // only calculate new position, if index has incremented: no mid-stroke update, as vibration is sufficiently fast
            if (index != _index) {
                if (_returnStroke == true) {
                    // long vibration distance on way out
                    // odd stroke is shaking out
                    if (index % 2) {  
                        // limit stroke to _depth - _stroke
                        _nextMove.stroke = max(_nextMove.stroke - _inVibrationDistance, _depth - _stroke);

                    // even stroke is shaking in
                    } else {
                        _nextMove.stroke = _nextMove.stroke + _outVibrationDistance;
                    }

                } else {
                    // long vibration distance on way in
                    // odd stroke is shaking out
                    if (index % 2) {  
                        _nextMove.stroke = _nextMove.stroke - _outVibrationDistance;

                    // even stroke is shaking in
                    } else {
                        // limit stroke to _depth
                        _nextMove.stroke = min(_nextMove.stroke + _inVibrationDistance, _depth);
                    }
                }
            }

            _index = index;
            return _nextMove;
        }
    protected:
        bool _returnStroke = false;
        int _inVibrationDistance = 0;
        int _outVibrationDistance = 0;
        int _strokeSpeed = 0;
        void _updateVibrationParameters() {
            // Empirical factor to compensate time losses due to finite acceleration.
            _strokeSpeed = int(5.0 * _stroke/_timeOfStroke);

            // Scale vibration amplitude from 3mm to 25mm with sensation
            _inVibrationDistance = (int)fscale(-100.0, 100.0, (float)(3.0*_stepsPerMM), (float)(25.0*_stepsPerMM), _sensation, 0.0);

            /* Calculate _outVibrationDistance to match with stroking speed
               d_out = d_in * (v_vib - v_stroke) / (v_vib + v_stroke)
               Formula neglects acceleration. Real timing will be slower due to finite acceleration & deceleration
            */
           _outVibrationDistance = _inVibrationDistance * (_maxSpeed - _strokeSpeed) / (_maxSpeed + _strokeSpeed);

#ifdef DEBUG_PATTERN
            Serial.println("_maxSpeed: " + String(_maxSpeed) + " _strokeSpeed: " + String(_strokeSpeed));
            Serial.println("inDist: " + String(_inVibrationDistance) + " outDist: " + String(_outVibrationDistance));
#endif
            
        }
};

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
            float seconds = (_minDurationSeconds + (sensation + 100.0f) * (_maxDurationSeconds - _minDurationSeconds) / 200.0f);
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

/**************************************************************************/
/*!
  @brief  Burst, then pull out. A run of fast full-length strokes (the
  burst), followed by one slow pull-out, then the run starts again.

  The Speed dial sets the burst: the in and out legs of the burst strokes
  run at the normal Simple Stroke profile for the dialed speed. The pull-out
  is the out leg of the LAST stroke in the run, stretched to take longer.
  Sensation sets how much slower: at -100 the pull-out takes 2x as long as a
  burst out-leg, at +100 it takes 10x as long (geometric in between, so the
  middle of the slider is about 4.5x). The burst length is the number of
  fast strokes per run (setBurstCount(), default 5), counted in whole
  in+out strokes, the final one of which ends in the slow pull-out.

  Stateless: the leg is derived from the stroke index alone, so there is no
  timer to drift and a skipped call cannot desync it. The StrokeEngine
  restarts the index at 0 whenever the pattern starts or is changed.

  Reversal note for the rotation axis: the burst legs reverse exactly as
  often as Simple Stroke at the same dial speed, so this pattern is NOT in
  main.cpp's vibration-heavy list and Screw Sync follows it normally.
*/
/**************************************************************************/
class BurstPullOut : public Pattern {
    public:
        BurstPullOut(const char *str) : Pattern(str) { _updatePullOutFactor(); }

        void setTimeOfStroke(float speed = 0) {
            // In & Out have same time, so we need to divide by 2
            _timeOfStroke = 0.5 * speed;
        }

        //! Sensation sets how much slower the pull-out is than a burst out-leg.
        void setSensation(float sensation) {
            _sensation = sensation;
            _updatePullOutFactor();
        }

        //! Number of fast strokes (in+out) per run, the last of which ends in the slow pull-out. Minimum 1.
        void setBurstCount(int count) { _burstCount = (count < 1) ? 1 : count; }

        //! Pull-out duration as a multiple of a burst out-leg: minFactor at Sensation -100, maxFactor at +100.
        void setPullOutFactorRange(float minFactor, float maxFactor) {
            _minFactor = (minFactor < 1.0f) ? 1.0f : minFactor;
            _maxFactor = (maxFactor < _minFactor) ? _minFactor : maxFactor;
            _updatePullOutFactor();
        }

        motionParameter nextTarget(unsigned int index) {
            unsigned int legsPerRun = 2 * (unsigned int)_burstCount;
            bool movingOut = (index % 2) != 0;
            bool isPullOut = movingOut && ((index % legsPerRun) == (legsPerRun - 1));

            // Time of this leg: a normal half-stroke, or the stretched pull-out.
            float legTime = isPullOut ? (_timeOfStroke * _pullOutFactor) : _timeOfStroke;

            // maximum speed of the trapezoidal motion
            _nextMove.speed = int(1.5 * _stroke / legTime);

            // acceleration to meet the profile (1/3 - 1/3 - 1/3)
            _nextMove.acceleration = int(3.0 * float(_nextMove.speed) / legTime);

            _nextMove.stroke = movingOut ? (_depth - _stroke) : _depth;
            _nextMove.skip = false;

#ifdef DEBUG_PATTERN
            if (isPullOut) {
                Serial.println("Burst pull-out, x" + String(_pullOutFactor, 2) + " slower");
            }
#endif

            _index = index;
            return _nextMove;
        }

    protected:
        int _burstCount = 5;
        float _minFactor = 2.0f;
        float _maxFactor = 10.0f;
        float _pullOutFactor = 4.5f;

        void _updatePullOutFactor() {
            // geometric between minFactor (Sensation -100) and maxFactor (Sensation +100)
            float t = (constrain(_sensation, -100.0f, 100.0f) + 100.0f) / 200.0f;
            _pullOutFactor = _minFactor * powf(_maxFactor / _minFactor, t);
        }
};

/**************************************************************************/
/*
  Array holding all different patterns. Please include any custom pattern here.
*/
/**************************************************************************/
static Pattern *patternTable[] = { 
  new SimpleStroke("Simple Stroke"),
  new TeasingPounding("Teasing or Pounding"),
  new RoboStroke("Robo Stroke"),
  new HalfnHalf("Half'n'Half"),
  new Deeper("Deeper"),
  new StopNGo("Stop'n'Go"),
  new Insist("Insist"),
  new JackHammer("Jack Hammer"),
  new StrokeNibbler("Stroke Nibbler"),
  new DepthPulse("Depth Pulse"),
  new BurstPullOut("Burst Pull-out")
  // <-- insert your new pattern class here!
 };

static const unsigned int patternTableSize = sizeof(patternTable) / sizeof(patternTable[0]);
