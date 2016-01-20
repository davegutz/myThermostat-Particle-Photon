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


double decimalTime(unsigned long *currentTime, char* tempStr);
void displayRandom(void);
void displayTemperature(int temp);
void getWeather(void);
void gotWeatherData(const char *name, const char *data);
void loadTemperature(void);
//double lookupTemp(double tim);
double modelTemperature(bool call, double OAT, double T);
double recoveryTime(double OAT);
void saveTemperature();
//double scheduledTemp(double hourDecimal, double recoTime, bool *reco);
void setupMatrix(Adafruit_8x8matrix m);
String tryExtractString(String str, const char* start, const char* end);

#endif
