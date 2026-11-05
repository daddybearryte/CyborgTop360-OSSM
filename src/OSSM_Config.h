/*
    User Config for OSSM - Reference board users should tweak this to match their personal build.
*/

#define EEPROM_SIZE 200
#define HW_VERSION 22 //divide by 10 for real hw version

// Homing mode. false = endstop switch on SERVO_ENDSTOP (IO22), homes toward the rear. true = sensorless (current sense), drives toward the FRONT stop first.
// Fixed here on purpose: the stored EEPROM hardware version no longer selects the homing mode.
#define USE_SENSORLESS_HOMING false

//#define INITIAL_SETUP //should only be defined at initial burn to configure HW version

#define OSSM_ID  1 //OSSM_ID Default can be changed with M5 Remote in the Future will be Saved in EPROOM
#define M5_ID 99 //M5_ID Default can be changed with M5 Remote in the Future will be Saved in EPROOM

/*
        Motion System Config
*/

#define STEP_PER_REV      4000      // Measured pulses/rev for this driver (did not match DIP table; trust measurement)
#define PULLEY_TEETH      20        // How many teeth has the pulley
#define BELT_PITCH        2         // What is the timing belt pitch in mm
#define MAX_RPM           3500.0    // Maximum RPM of motor
#define MAX_ACCELERATION  20000     // Maximum linear acceleration in mm/s²

// This is in millimeters, and is what's used to define how much of
// your rail is usable.
// The absolute max your OSSM would have is the distance between the belt attachments subtract
// the linear block holder length (75mm on OSSM)
#define MAX_STROKEINMM 150.0  // Real physical travel from one hard endstop to the other
#define STROKEBOUNDARY 10.0  // Safe distance the motion is constrained to avoiding crashes

// Calculation Aid:
#define STEP_PER_MM       STEP_PER_REV / (PULLEY_TEETH * BELT_PITCH)
#define MAX_SPEED         (MAX_RPM / 60.0) * PULLEY_TEETH * BELT_PITCH

#define DEPTH_RESULTION 5 // Depth Resultion in mm per encoder Klick
#define STROKE_RESULTION 5 // STROKE Resultion in mm per encoder Klick
#define ENCODER_RESULTION 36 // Klicks per turn
#define USER_SPEEDLIMIT 900 // Speed in Cycles (in & out) per minute.

// --- Rotation axis config ---
#define ROTATION_PULSES_PER_REV   4000    // 400 pulses/rev DIP x ~10:1 reduction (measured)
#define ROTATION_MAX_RPM          100.0   // raise once verified unloaded
#define ROTATION_BURST_RPM        120.0   // absolute rotation burst ceiling in RPM at full dial; boot default, the remote Tweak screen changes it live
#define ROTATION_OVERDRIVE_FACTOR (ROTATION_BURST_RPM / ROTATION_MAX_RPM)   // ceiling = MAX_RPM x this; patterns reach it only with Overdrive on
#define ROTATION_MAX_ACCEL_RPM_PER_S 300.0   // output slew limit in RPM per second, 0 = off. Lower it if the driver still faults.
#define ROTATION_REVERSAL_SAFE_RPM   20.0    // direction flips only at or below this RPM; the axis slows to it first
#define ROTATION_INVERT_DIRECTION false   // flip if it spins backwards (direct drive is active-low)
#define ROTATION_ENABLE_ACTIVE_HIGH true  // unused while ENA is unwired

// Stroke Sensation response curve, applied where the remote's SENSATION command arrives.
// 1.0 = linear (off). Below 1.0 the middle of the slider does more: at 0.6 a slider reading of 33 acts like about 51.
// Not applied while Set Depth Fancy is active (there Sensation positions the carriage).
#define STROKE_SENSATION_CURVE    0.6

// The minimum value of the pot in percent
// prevents noisy pots registering commands when turned down to zero by user
const float commandDeadzonePercentage = 1.0f;
