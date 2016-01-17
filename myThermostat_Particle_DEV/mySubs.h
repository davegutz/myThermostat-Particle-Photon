#include "application.h"
//#include "adafruit-led-backpack.h"
#include "SparkIntervalTimer.h"
#define NCH         4                       // Number of temp changes in daily sched (4)
#define GMT         -5                      // Enter time different to zulu (does not respect DST)
#define DIM_DELAY   3000UL                  // LED display timeout to dim, ms

/*
#ifndef BARE_PHOTON
  static Adafruit_8x8matrix   matrix1;                // Tens LED matrix
  static Adafruit_8x8matrix   matrix2;                // Ones LED matrix
#endif
static bool                   call            = false;// Heat demand to relay control
static IntervalTimer          myTimerD;               // To dim display
*/

// Time to trigger setting change
static float hourCh[7][NCH] = {
    6, 8, 16, 22,   // Sat
    6, 8, 16, 22,   // Sun
    4, 7, 16, 22,   // Mon
    4, 7, 16, 22,   // Tue
    4, 7, 16, 22,   // Wed
    4, 7, 16, 22,   // Thu
    4, 7, 16, 22    // Fri
};
// Temp setting after change in above table.   Change holds until next
// time trigger.  Temporarily over-ridden either by pot or web adjustments.
static const float tempCh[7][NCH] = {
    68, 62, 68, 62, // Sat
    68, 62, 68, 62, // Sun
    68, 62, 68, 62, // Mon
    68, 62, 68, 62, // Tue
    68, 62, 68, 62, // Wed
    68, 62, 68, 62, // Thu
    68, 62, 68, 62  // Fri
};

//void displayRandom(void);
double lookupTemp(double tim);
double scheduledTemp(double hourDecimal, double recoTime, bool *reco);
