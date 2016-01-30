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
  #define FILTER_DELAY   5000UL             // In range of tau/4 - tau/3  * 1000, ms
#endif
#define MINSET      50                      // Minimum setpoint allowed (50), F
#define NOMSET      68                      // Nominal setpoint for modeling etc, F
#define MAXSET      72                      // Maximum setpoint allowed (72), F
#define WEATHER_WAIT      900UL             // Time to wait for weather webhook, ms


// Embedded model class
class HouseHeat
{
private:
  String name_;     // Object name label
  double Ha_;       // Air to wall constant, BTU/sec/F
  double Hc_;       // Core to air constant, BTU/sec/F
  double Hf_;       // Firing constant, BTU/sec/F
  double Ho_;       // Wall to outside constant, BTU/sec/F
  double Rn_;       // Low boiler reset curve OAT break, F
  double Rx_;       // High boiler reset curve OAT break, F
  double Ta_;       // Air temp, F
  double Tc_;       // Core heater temp, F
  double Tn_;       // Low boiler reset curve setpoint break, F
  double Tx_;       // High boiler reset curve setpoint break, F
  double Tw_;       // Outside wall temp, F
public:
  HouseHeat(void);
  HouseHeat(const String name, const double Ha, const double Hc, const double Hf, const double Ho, \
    const double Rn, const double Rx, const double Tn, const double Tx);
  double update(const bool RESET, const double T, const double temp,  const double duty, const double otherHeat, const double OAT);
};

double  decimalTime(unsigned long *currentTime, char* tempStr);
void    displayRandom(void);
void    displayTemperature(int temp);
void    getWeather(void);
void    gotWeatherData(const char *name, const char *data);
double  houseControl(const bool RESET, const double duty, const double Ta_Sense,\
   const double Ta_Obs, const double T);
void    loadTemperature(void);
double  lookupTemp(double tim);
double  houseEmbeddedModel(const double temp, const int RESET, const double duty, const double otherHeat, const double OAT, const double T);
double  recoveryTime(double OAT);
void    saveTemperature();
double  scheduledTemp(double hourDecimal, double recoTime, bool *reco);
void    setupMatrix(Adafruit_8x8matrix m);
String  tryExtractString(String str, const char* start, const char* end);

#endif
