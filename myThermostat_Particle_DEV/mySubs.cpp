#include "application.h"
#include "mySubs.h"
#include "pixmaps.h"

static int verbose;

/*
// Put randomly placed activity pattern on LED display to preserve life.
void displayRandom(void)
{
#ifndef BARE_PHOTON
  matrix1.clear();
  matrix2.clear();
  if (!call)
  {
    matrix1.setCursor(1, 0);
    matrix1.drawBitmap(0, 0, randomDot(), 8, 8, LED_ON);
  }
  else
  {
    matrix2.setCursor(1, 0);
    matrix2.drawBitmap(0, 0, randomPlus(), 8, 8, LED_ON);
  }
  matrix1.setBrightness(1);  // 1-15
  matrix2.setBrightness(1);  // 1-15
  matrix1.writeDisplay();
  matrix2.writeDisplay();
#endif
  // Reset clock
  myTimerD.resetPeriod_SIT(DIM_DELAY, hmSec);
}
*/

// Lookup temp at time
double lookupTemp(double tim)
{
    // tim is decimal hours in week, 0 = midnight Sunday
    int day = tim/24;  // Day known apriori
    int num;
    if ( ((day==0) & (tim< hourCh[0][0]))   |\
         ((day==6) & (tim>=hourCh[day][NCH-1])) )
    {
        day = 6;
        num = NCH-1;
    }
    else
    {
        int i;
        for (i=0; i<7*NCH; i++)
        {
            day = i/NCH;
            num = i-day*NCH;
            if ( !(tim>hourCh[day][num]) )
            {
                i--;
                day = i/NCH;
                num = i-day*NCH;
                break;
            }
        }
    }
    return (tempCh[day][num]);
}


// Calculate scheduled temperature
double scheduledTemp(double hourDecimal, double recoTime, bool *reco)
{
    // Calculate ecovery and wrap time
    double hourDecimalShift = hourDecimal + recoTime;
    if ( hourDecimalShift >7*24 ) hourDecimalShift -= 7*24;

    // Find spot in schedules
    double tempSchd      = lookupTemp(hourDecimal);
    double tempSchdShift = lookupTemp(hourDecimalShift);
    if (verbose>5) Serial.printf("hour=%f, recoTime=%f, tempSchd=%f, tempSchdShift=%f\n",\
                                            hourDecimal, recoTime, tempSchd, tempSchdShift);
    *reco               = tempSchdShift>tempSchd;
    tempSchd            = max(tempSchd, tempSchdShift); // Turn on early but not turn off early
    return tempSchd;
}
