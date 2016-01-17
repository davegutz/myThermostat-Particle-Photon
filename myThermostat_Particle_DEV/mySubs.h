#ifndef _MY_SUBS_H
#define _MY_SUBS_H

#include "application.h"
#include "pixmaps.h"
#include "adafruit-led-backpack.h"
#include "SparkIntervalTimer.h"
#define NCH         4                       // Number of temp changes in daily sched (4)
#define GMT         -5                      // Enter time different to zulu (does not respect DST)
#define DIM_DELAY   3000UL                  // LED display timeout to dim, ms
#ifndef BARE_PHOTON
  #define FILTER_DELAY   5000UL             // In range of tau/4 - tau/3  * 1000, ms
#else
  #define FILTER_DELAY   3500UL             // In range of tau/4 - tau/3  * 1000, ms
#endif
#define MINSET      50                      // Minimum setpoint allowed (50), F
#define MAXSET      72                      // Maximum setpoint allowed (72), F
#define WEATHER_WAIT      900UL             // Time to wait for weather webhook, ms

#ifndef NO_WEATHER_HOOK
  static int                  badWeatherCall  = 0;    // webhook lookup counter
#endif
  static const   int          EEPROM_ADDR     = 10;   // Flash address
  static bool                 held            = false;// Web toggled permanent and acknowledged
#ifndef BARE_PHOTON
  static Adafruit_8x8matrix   matrix1;                // Tens LED matrix
  static Adafruit_8x8matrix   matrix2;                // Ones LED matrix
#endif
//static bool                   call            = false;// Heat demand to relay control
static IntervalTimer          myTimerD;               // To dim display
  static int                  set             = 62;   // Selected sched, F
  static double               tempf         = 30;     // webhook OAT, deg F
  static double               Thouse;                 // House bulk temp, F
#ifndef NO_WEATHER_HOOK
  static long                 updateweatherhour= 0;   // Last hour weather updated
#endif
#ifndef NO_WEATHER_HOOK
  static bool                 weatherGood     = false;// webhook OAT lookup successful, T/F
#endif
  static int                  webDmd          = 62;   // Web sched, F
  static bool                 webHold         = false;// Web permanence request

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

double decimalTime(unsigned long *currentTime, char* tempStr);
//void displayRandom(void);
void displayTemperature(int temp);
void getWeather(void);
void gotWeatherData(const char *name, const char *data);
void loadTemperature(void);
double lookupTemp(double tim);
double modelTemperature(bool call, double OAT, double T);
double recoveryTime(double OAT);
void saveTemperature();
double scheduledTemp(double hourDecimal, double recoTime, bool *reco);
void setupMatrix(Adafruit_8x8matrix m);
String tryExtractString(String str, const char* start, const char* end);

#endif
