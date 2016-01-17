/* myThermostat.ino
  Control home heating (not cooling) with a potentiometer, a web interface
  (Blynk), and by schedules.  This is written for a Particle Photon
  target.  The electrical schematic is the same as the Spark Nest Thermostat
  project with I2C 4.7K pullup resistors added and the solid state relay swapped out for
  a Reed relay to control a Triangle Tube Prestige furnace interface (Honeywell
  dial thermostat compatible).

  see README.md for more information

  11-Nov-2015   Dave Gutz   Created
  17-Jan-2016   Dave Gutz   Deleted SparkTime and UDP stuff for stability.

  TODO:::::::::::::::::::
Run Control frame using an interrupt so that control possible even if something
else hangs.  This must include read of temp/hum sensor too.

*/

//Sometimes useful for debugging
//#pragma SPARK_NO_PREPROCESSOR
//
// Standard
#include "application.h"                    // For Particle code classes
SYSTEM_THREAD(ENABLED);                     // Make sure heat system code always run regardless of network status
//
// Disable flags if needed, usually commented
#define BARE_PHOTON                       // Run bare photon for testing.  Bare photon without this goes dark or hangs trying to write to I2C
//#define NO_WEATHER_HOOK                   // Turn off webhook weather lookup.  Will get default OAT = 30F
//#define WEATHER_BUG                       // Turn on bad weather return for debugging
//#define NO_BLYNK                          // Turn off Blynk functions.  Interact using Particle cloud
//#define NO_PARTICLE                       // Turn off Particle cloud functions.  Interact using Blynk.
//
// Usually defined.
//
// Test feature usually commented
//#define  FAKETIME                         // For simulating rapid time passing of schedule
//
// Constants always defined
#define BLYNK_TIMEOUT_MS 2000UL             // Network timeout in ms;  default provided in BlynkProtocol.h is 2000
#define CONTROL_DELAY    1000UL             // Control law wait (1000), ms
#define PUBLISH_DELAY    10000UL            // Time between cloud updates (10000), ms
#define READ_DELAY       5000UL             // Sensor read wait (5000, 100 for stress test), ms
#define QUERY_DELAY      15000UL            // Web query wait (15000, 100 for stress test), ms
#define DIM_DELAY        3000UL             // LED display timeout to dim, ms
#define DISPLAY_DELAY    300UL              // LED display scheduling frame time, ms
#define HEAT_PIN    A1                      // Heat relay output pin on Photon (A1)
#define HYST        0.5                     // Heat control law hysteresis (0.5), F
#define POT_PIN     A2                      // Potentiometer input pin on Photon (A2)
#define TEMP_SENSOR 0x27                    // Temp sensor bus address (0x27)
#define TEMPCAL     -4                      // Calibrate temp sense (0), F
#define MINSET      50                      // Minimum setpoint allowed (50), F
#define MAXSET      72                      // Maximum setpoint allowed (72), F
#define ONE_DAY_MILLIS (24*60*60*1000)      // Number of milliseconds in one day (24*60*60*1000)
#include "mySubs.h"
//
// Dependent includes.   Easier to debug code if remove unused include files
#include "SparkIntervalTimer.h"
#ifndef NO_BLYNK
  #include "blynk.h"
  #include "BlynkHandlers.h"
#endif
#include <math.h>
//#include "adafruit-led-backpack.h"
//#include "pixmaps.h"
#include "myAuth.h"
/* This file myAuth.h is not in Git repository because it contains personal information.
Make it yourself.   It should look like this, with your personal authorizations:
(Note:  you don't need a valid number for one of the blynkAuth if not using it.)
#ifndef BARE_PHOTON
  const   String      blynkAuth     = "4f1de4949e4c4020882efd3e61XXX6cd"; // Photon thermostat
#else
  const   String      blynkAuth     = "d2140898f7b94373a78XXX158a3403a1"; // Bare photon
#endif
*/
#include "myFilters.h"

using namespace std;

// Global variables
enum                Mode {POT, WEB, SCHD};  // To keep track of mode
bool                call            = false;// Heat demand to relay control
double              callCount;              // Floating point of bool call for calculation
#ifndef BARE_PHOTON
  double            compGain        = 150.; // Temperature compensation gain, deg/(deg/sec)
#else
  double            compGain        = 20.;  // Temperature compensation gain, deg/(deg/sec)
#endif
Mode                controlMode     = POT;  // Present control mode
static  uint8_t     displayCount    = 0;    // Display frame number to execute
const   int         EEPROM_ADDR     = 10;   // Flash address
bool                held            = false;// Web toggled permanent and acknowledged
String              hmString        = "00:00"; // time, hh:mm
static double       controlTime     = 0.0;  // Decimal time, hour
int                 hum             = 0;    // Relative humidity integer value, %
int                 I2C_Status      = 0;    // Bus status
bool                lastHold        = false;// Web toggled permanent and acknowledged
unsigned long       lastSync     = millis();// Sync time occassionally.   Recommended by Particle.
const   int         LED             = D7;   // Status LED
#ifndef BARE_PHOTON
  Adafruit_8x8matrix  matrix1;              // Tens LED matrix
  Adafruit_8x8matrix  matrix2;              // Ones LED matrix
#endif
IntervalTimer       myTimerD;               // To dim display
IntervalTimer       myTimer7;               // To shift LED pattern for life
int                 numTimeouts     = 0;    // Number of Particle.connect() needed to unfreeze
double              OAT             = 30;   // Outside air temperature, F
int                 potDmd          = 0;    // Pot value, deg F
int                 potValue        = 62;   // Dial raw value, F
char                publishString[40];      // For uptime recording
bool                reco;                   // Indicator of recovering on cold days by shifting schedule
int                 schdDmd         = 62;   // Sched raw value, F
int                 set             = 62;   // Selected sched, F
#ifndef NO_PARTICLE
  String            statStr("WAIT...");     // Status string
#endif
#ifndef BARE_PHOTON
  double            tau           =  20.0;  // Rate filter time constant, sec, ~1/10 observed home time constant
#else
  double            tau           = 12.0;   // Rate filter time constant, sec, ~1/10 observed home time constant
#endif
double              temp          = 65.0;   // Sensed temp, F
double              tempComp;               // Sensed compensated temp, F
double              updateTime      = 0.0;  // Control law update time, sec
static const int    verbose         = 3;    // Debug, as much as you can tolerate
int                 webDmd          = 62;   // Web sched, F
bool                webHold         = false;// Web permanence request

// Schedules
bool hourChErr = false;
// Rate filter.
// Exponential chosen for this because it does not suffer from aliasing problems
// so users not used to tuning digital filters may set tau and T at will.
// There are some general guidelines for setting those in their comments in this file.
RateLagExp*  rateFilter;

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


// Handler for the display dimmer timer, called automatically
void onTimerDim(void)
{
#ifndef BARE_PHOTON
  displayRandom();
#endif
}

// Display the temperature setpoint on LED matrices.   All the ways to set temperature
// are displayed on the matrices.  It's cool to see the web and schedule adjustments change
// the display briefly.
void displayTemperature(int temp)
{
#ifndef BARE_PHOTON
    char ones = abs(temp) % 10;
    char tens =(abs(temp) / 10) % 10;
    matrix1.clear();
    matrix1.setCursor(0, 0);
    matrix1.write(tens + '0');
    matrix1.setBrightness(1);  // 1-15
    matrix1.blinkRate(0);      // 0-3
    matrix1.writeDisplay();
    matrix2.clear();
    matrix2.setCursor(0, 0);
    matrix2.write(ones + '0');
    matrix2.setBrightness(1);  // 1-15
    matrix2.blinkRate(0);      // 0-3
    matrix2.writeDisplay();
#endif
    // Reset clock
    myTimerD.resetPeriod_SIT(DIM_DELAY, hmSec);
}

// Display the temperature setpoint on LED matrices.   All the ways to set temperature
// are displayed on the matrices.  It's cool to see the web and schedule adjustments change
// the display briefly.
void displayMessage(const String str)
{
#ifndef BARE_PHOTON
    char first  = str[0];
    char second = str[1];
    matrix1.clear();
    matrix1.setCursor(0, 0);
    matrix1.write(first);
    matrix1.setBrightness(1);  // 1-15
    matrix1.blinkRate(0);      // 0-3
    matrix1.writeDisplay();
    matrix2.clear();
    matrix2.setCursor(0, 0);
    matrix2.write(second);
    matrix2.setBrightness(1);  // 1-15
    matrix2.blinkRate(0);      // 0-3
    matrix2.writeDisplay();
#endif
    // Reset clock
    myTimerD.resetPeriod_SIT(DIM_DELAY, hmSec);
}

// Save temperature setpoint to flash for next startup.   During power
// failures the thermostat will reset to the condition it was in before
// the power failure.   Filter initialized to sensed temperature (lose anticipation briefly
// following recovery from power failure).
void saveTemperature()
{
    uint8_t values[4] = { (uint8_t)set, (uint8_t)held, (uint8_t)webDmd, (uint8_t)(roundf(Thouse)) };
    EEPROM.put(EEPROM_ADDR, values);
}

// Setup function to Load the saved settings so can resume after power failure
// or software reflash.  Note:  flash may return nonsense such as for a brand
// new Photon unit and we'll need some safe (furnace off) default values.
void loadTemperature()
{
    if (verbose>0) Serial.println("Loading and displaying temperature from flash");
    uint8_t values[4];
    EEPROM.get(EEPROM_ADDR, values);
    //
    set     = values[0];
    if ( (set     > MAXSET  ) | (set     < MINSET  ) ) set     = MINSET;
    displayTemperature(set);
    //
    webHold = values[1];
    if ( (webHold >    1    ) | (webHold <    0    ) ) webHold =  0;
    //
    webDmd  = (int)values[2];
    if ( (webDmd  > MAXSET  ) | (webDmd  < MINSET  ) ) webDmd  = MINSET;
    //
    Thouse  = values[3];
    if ( (Thouse  > MAXSET+1) | (Thouse  < MINSET-1) ) Thouse  = (MAXSET+MINSET)/2;
}

// Process a new temperature setting.   Display and save it.
int setSaveDisplayTemp(int t)
{
    set = t;
    switch(controlMode)
    {
        case POT:   displayTemperature(set); displayCount=0; break;
        case WEB:   break;
        case SCHD:  break;
    }
    saveTemperature();
    return set;
}


#ifndef NO_BLYNK
// Attach a Slider widget to the Virtual pin 4 IN in your Blynk app
// - and control the web desired temperature.
// Note:  there are separate virtual IN and OUT in Blynk.
BLYNK_WRITE(V4) {
    if (param.asInt() > 0)
    {
        webDmd = (int)param.asDouble();
    }
}
#endif
#ifndef NO_PARTICLE
int particleSet(String command)
{
  int possibleSet = atoi(command);
  if (possibleSet >= MINSET && possibleSet <= MAXSET)
  {
      webDmd = possibleSet;
      return possibleSet;
  }
  else return -1;
}
#endif

#ifndef NO_BLYNK
// Attach a switch widget to the Virtual pin 6 in your Blynk app - and demand continuous web control
// Note:  there are separate virtual IN and OUT in Blynk.
BLYNK_WRITE(V6) {
    webHold = param.asInt();
}
#endif
#ifndef NO_PARTICLE
int particleHold(String command)
{
  if (command == "HOLD")
  {
    webHold = true;
    return 1;
  }
  else
  {
     webHold = false;
     return 0;
  }
}
#endif

// Setup
void setup()
{
//    SYSTEM_MODE(SEMI_AUTOMATIC);// this line causes SOS + hard fault (don't understand why)
  Serial.begin(9600);
  delay(2000); // Allow board to settle
  pinMode(LED, OUTPUT);               // sets pin as output
  #ifndef NO_PARTICLE
    statStr.reserve(120);
    Particle.variable("stat", statStr);
  #endif
  pinMode(HEAT_PIN,   OUTPUT);
  pinMode(POT_PIN,    INPUT);
  #ifndef BARE_PHOTON
    Wire.setSpeed(CLOCK_SPEED_100KHZ);
    Wire.begin();
    matrix1.begin(0x70);
    matrix2.begin(0x71);
    setupMatrix(matrix1);
    setupMatrix(matrix2);
    setSaveDisplayTemp(0);            // Assure user reset happened
    delay(2000);
  #else
    delay(10);
  #endif
  loadTemperature();
  myTimerD.begin(onTimerDim, DIM_DELAY, hmSec);

  // Time schedule convert and check
  for (int day=0; day<7; day++)
    for (int num=0; num<NCH; num++)
  {
      if (hourCh[day][num] > 24) hourChErr = true;
      hourCh[day][num] = hourCh[day][num] + float(day)*24.0;
  }

  for (int day=0; day<7; day++)
    for (int num=0; num<(NCH-1); num++)
  {
      if (hourCh[day][num] >= hourCh[day][num+1]) hourChErr = true;
  }

  // OAT
  // Lets listen for the hook response
  #ifndef NO_WEATHER_HOOK
    Spark.subscribe("hook-response/get_weather", gotWeatherData, MY_DEVICES);
  #endif

  // Rate filter
  rateFilter  = new RateLagExp(float(FILTER_DELAY)/1000.0, tau, -0.1, 0.1);

  // Begin
  Particle.connect();
  #ifndef NO_PARTICLE
    Particle.function("HOLD", particleHold);
    Particle.function("SET",  particleSet);
  #endif
  #ifndef NO_BLYNK
    Blynk.begin(blynkAuth.c_str());
  #endif
  #ifndef NO_WEATHER_HOOK
    if (verbose>0)Serial.print("initializing weather...");
    Serial.flush();
    getWeather();
  #endif
  if (verbose>1) Serial.printf("End setup()\n");
}


// Loop
void loop()
{
    unsigned long           currentTime;        // Time result
    unsigned long           now = millis();     // Keep track of time
    bool                    control;            // Control sequence, T/F
    bool                    display;            // LED display sequence, T/F
    bool                    filter;             // Filter for temperature, T/F
    bool                    publishAny;         // Publish, T/F
    bool                    publish1;           // Publish, T/F
    bool                    publish2;           // Publish, T/F
    bool                    publish3;           // Publish, T/F
    bool                    publish4;           // Publish, T/F
    bool                    query;              // Query schedule and OAT, T/F
    bool                    read;               // Read, T/F
    bool                    checkPot;            // Display to LED, T/F
    static double           lastHour     = 0.0; // Past used time value,  hours
    static unsigned long    lastControl  = 0UL; // Last control law time, ms
    static unsigned long    lastDisplay  = 0UL; // Las display time, ms
    static unsigned long    lastFilter   = 0UL; // Last filter time, ms
    static unsigned long    lastPublish1 = 0UL; // Last publish time, ms
    static unsigned long    lastPublish2 = 0UL; // Last publish time, ms
    static unsigned long    lastPublish3 = 0UL; // Last publish time, ms
    static unsigned long    lastPublish4 = 0UL; // Last publish time, ms
    static unsigned long    lastQuery    = 0UL; // Last read time, ms
    static unsigned long    lastRead     = 0UL; // Last read time, ms
    static int              RESET        = 1;   // Dynamic initialization flag, T/F
    static double           tempRate;           // Rate of change of temp, deg F/sec

    // Sequencing
    #ifndef NO_BLYNK
      Blynk.run();
    #endif
    if (millis() - lastSync > ONE_DAY_MILLIS)
    {
      // Request time synchronization from the Particle Cloud once per day
      Particle.syncTime();
      lastSync = millis();
    }

    filter    = ((now-lastFilter)>=FILTER_DELAY) || RESET>0;
    if ( filter )   lastFilter    = now;

    publish1  = ((now-lastPublish1) >= PUBLISH_DELAY*4);
    if ( publish1 ) lastPublish1  = now;

    publish2  = ((now-lastPublish2) >= PUBLISH_DELAY*4)  && ((now-lastPublish1) >= PUBLISH_DELAY);
    if ( publish2 ) lastPublish2  = now;

    publish3  = ((now-lastPublish3) >= PUBLISH_DELAY*4)  && ((now-lastPublish1) >= PUBLISH_DELAY*2);
    if ( publish3 ) lastPublish3  = now;

    publish4  = ((now-lastPublish4) >= PUBLISH_DELAY*4)  && ((now-lastPublish1) >= PUBLISH_DELAY*3);
    if ( publish4 ) lastPublish4  = now;

    publishAny  = publish1 || publish2 || publish3 || publish4;

    read    = ((now-lastRead) >= READ_DELAY) && !publishAny;
    if ( read     ) lastRead      = now;

    query   = ((now-lastQuery)>= QUERY_DELAY) && !read;
    if ( query    ) lastQuery     = now;

    display   = ((now-lastDisplay) >= DISPLAY_DELAY) && !query;
    if ( display ) lastDisplay    = now;

    unsigned long deltaT = now - lastControl;
    control = (deltaT >= CONTROL_DELAY) && !display;
    if ( control  )
    {
      updateTime    = float(deltaT)/1000.0 + float(numTimeouts)/100.0;
      lastControl   = now;
    }

    checkPot   = !control && !query  && !read && !publishAny;

    #ifndef NO_WEATHER_HOOK
      // Get OAT webhook
      if ( query    )
      {
        unsigned long           then = millis();     // Keep track of time
        getWeather();
        unsigned long           now = millis();     // Keep track of time
        if (verbose>0) Serial.printf("weather update=%f\n", float(now-then)/1000.0);
        if ( weatherGood )
        {
          OAT = tempf;
        }
        if (verbose>5) Serial.printf("OAT=%f at %s\n", OAT, hmString.c_str());
      }
    #endif

    // Read sensors
    if ( read )
    {
        #ifndef BARE_PHOTON
          Wire.beginTransmission(TEMP_SENSOR);
          Wire.endTransmission();
          delay(40);
          Wire.requestFrom(TEMP_SENSOR, 4);
          Wire.write(byte(0));
          uint8_t b   = Wire.read();
          I2C_Status  = b >> 6;
          //
          int rawHum  = (b << 8) & 0x3f00;
          rawHum      |=Wire.read();
          hum         = roundf(rawHum / 163.83);
          //
          int rawTemp = (Wire.read() << 6) & 0x3fc0;
          rawTemp     |=Wire.read() >> 2;
          temp        = (float(rawTemp)*165.0/16383.0 - 40.0)*1.8 + 32.0 + TEMPCAL; // convert to fahrenheit and calibrate
        #else
          delay(41); // Usual U2C_time
          temp    = modelTemperature(call, OAT, READ_DELAY/1000);
        #endif
    }
    if ( filter )
    {
        tempRate = rateFilter->calculate(temp, RESET);
        if ( verbose > 4) Serial.printf("%f %f %f %f %f %f\n", rateFilter->a(), rateFilter->b(), rateFilter->c(), rateFilter->rstate(), rateFilter->lstate(), tempRate );
//        tempRate = 0.0;
        RESET = 0;
    }
    if ( read ) tempComp  = temp + tempRate*compGain;

    // Interrogate pot; run fast for good tactile feedback
    // my pot puts out 2872 - 4088 observed using following
    #ifndef BARE_PHOTON
      potValue    = 4095 - analogRead(POT_PIN);
    #else
      potValue    = 2872;
    #endif
    potDmd      = roundf((float(potValue)-2872)/(4088-2872)*26+47);


    // Interrogate schedule
    if ( query    )
    {
        double recoTime = recoveryTime(OAT);
        schdDmd = scheduledTemp(controlTime, recoTime, &reco);
    }

    // Scheduling logic
    // 1.  Pot has highest priority
    //     a.  Pot will not hold past next schedule change
    //     b.  Web change will override it
    // 2.  Web Blynk has next highest priority
    //     a.  Web will hold only if HOLD is on
    //     b.  Web will HOLD indefinitely.
    //     c.  When Web is HELD, all other inputs are ignored
    // 3.  Finally the schedule gets it's say
    //     a.  Holds last number until time at next change
    //
    // Notes:
    // i.  webDmd is transmitted by Blynk to Photon only when it changes
    // ii. webHold is transmitted periodically by Blynk to Photon

    // Initialize scheduling logic - don't change on boot
    static int      lastChangedPot      = potValue;
    static int      lastChangedWebDmd   = webDmd;
    static int      lastChangedSched    = schdDmd;


    // If user has adjusted the potentiometer (overrides schedule until next schedule change)
    // Use potValue for checking because it has more resolution than the integer potDmd
    if ( fabsf(potValue-lastChangedPot)>16 && checkPot )  // adjust from 64 because my range is 1214 not 4095
    {
        controlMode     = POT;
        int t = min(max(MINSET, potDmd), MAXSET);
        setSaveDisplayTemp(t);
        held = false;  // allow the pot to override the web demands.  HELD allows web to override schd.
        if (verbose>0) Serial.printf("Setpoint based on pot:  %ld\n", t);
        lastChangedPot = potValue;
    }
    //
    // Otherwise if web Blynk has adjusted setpoint (overridden temporarily by pot, until next web adjust)
    // The held construct ensures that temp setting latched in by HOLD in Blynk cannot be accidentally changed
    // The webHold construct ensures that pushing HOLD in Blynk causes control to snap to the web demand
    else if ( ((abs(webDmd-lastChangedWebDmd)>0)  & (!held)) | (webHold & (webHold!=lastHold)) )
    {
        controlMode     = WEB;
        int t = min(max(MINSET, webDmd), MAXSET);
        setSaveDisplayTemp(t);
        if (verbose>0) Serial.printf("Setpoint based on web:  %ld\n", t);
        lastChangedWebDmd   = webDmd;
    }
    //
    // Otherwise if schedule has adjusted setpoint (overridden temporarily by pot or web, until next schd adjust)
    else if ( (fabsf(schdDmd-lastChangedSched)>0) & (!held) )
    {
        controlMode     = SCHD;
        int t = min(max(MINSET, schdDmd), MAXSET);
        if (hourChErr)
        {
            Serial.println("***Table error, ignoring****");
        }
        else
        {
            setSaveDisplayTemp(t);
            if (verbose>0) Serial.printf("Setpoint based on schedule:  %ld\n", t);
        }
        lastChangedSched = schdDmd;
    }
    if (webHold!=lastHold)
    {
        lastHold    = webHold;
        held        = webHold;
        saveTemperature();
    }


    // Display Sequencing
    // Run a different frame each time in here
    if ( display )
    {
      switch ( displayCount++ )
      {
        case 0-10:
          break;      // Hold.  Useful for freezing display after adjusting POT
        case 11:
          displayMessage("S=");
          break;
        case 12:
          displayTemperature(set);
          break;
        case 14:
          displayMessage("T=");
          break;
        case 15:
          displayTemperature(roundf(temp));
          break;
        case 17:
          displayMessage("O=");
          break;
        case 18:
          displayTemperature(OAT);
          break;
        case 20:
          displayMessage("H=");
          break;
        case 21:
          displayTemperature(hum);
          break;
        case 23:
          displayRandom();
          displayCount = 0;
          break;
        }
    }


    // Control law.   Simple on/off but update time structure ('control' calculation) provided for
    // dynamic logic when needed
    if ( control )
    {
      if (verbose>4) Serial.printf("Control\n");
      char  tempStr[8];                           // time, hh:mm
      controlTime = decimalTime(&currentTime, tempStr);
      //updateTime  = float(controlTime-lastHour)*3600.0 + float(numTimeouts)/100.0;
      lastHour    = controlTime;
      hmString    = String(tempStr);
      bool callRaw;
      if ( call )
      {
          callRaw    = ( (float(set)+float(HYST))  > tempComp);
      }
      else
      {
          callRaw    = ( (float(set)-float(HYST))  > tempComp );
      }
      // 5 update persistence for call change to help those with fat fingers
      callCount   += max(min((float(callRaw)-callCount),  0.2), -0.2);
      callCount   =  max(min(callCount,                   1.0),  0.0);
      call        =  callCount >= 1.0;
      digitalWrite(HEAT_PIN, call);
      digitalWrite(LED, call);
    }


    // Publish
    if ( publish1 || publish2 || publish3 || publish4 )
    {
      char  tmpsStr[100];
      sprintf(tmpsStr, "%s--> CALL %d | SET %d | TEMP %7.3f | TEMPC %7.3f | HUM %d | HELD %d | T %5.2f | POT %d | LWEB %d | SCH %d | OAT %4.1f\0", \
      hmString.c_str(), call, set, temp, tempComp, hum, held, updateTime, potDmd, lastChangedWebDmd, schdDmd, OAT);
      #ifndef NO_PARTICLE
        statStr = String(tmpsStr);
      #endif
      if (verbose>1) Serial.println(tmpsStr);
      if ( Particle.connected() )
      {
          unsigned nowSec = now/1000UL;
          unsigned sec = nowSec%60;
          unsigned min = (nowSec%3600)/60;
          unsigned hours = (nowSec%86400)/3600;
          sprintf(publishString,"%u:%u:%u",hours,min,sec);
          Spark.publish("Uptime",publishString);
          Spark.publish("stat", tmpsStr);
          #ifndef NO_BLYNK
            if (publish1)
            {
              if (verbose>4) Serial.printf("Blynk write1\n");
              Blynk.virtualWrite(V0,  call);
              Blynk.virtualWrite(V1,  set);
              Blynk.virtualWrite(V2,  temp);
              Blynk.virtualWrite(V3,  hum);
              Blynk.virtualWrite(V4,  tempComp);
            }
            if (publish2)
            {
              if (verbose>4) Serial.printf("Blynk write2\n");
              Blynk.virtualWrite(V5,  held);
              Blynk.virtualWrite(V7,  controlTime);
              Blynk.virtualWrite(V8,  updateTime);
              Blynk.virtualWrite(V9,  potDmd);
            }
            if (publish3)
            {
              if (verbose>4) Serial.printf("Blynk write3\n");
              Blynk.virtualWrite(V10, lastChangedWebDmd);
              Blynk.virtualWrite(V11, set);
              Blynk.virtualWrite(V12, schdDmd);
              Blynk.virtualWrite(V13, temp);
            }
            if (publish4)
            {
              if (verbose>4) Serial.printf("Blynk write4\n");
              Blynk.virtualWrite(V14, I2C_Status);
              Blynk.virtualWrite(V15, hmString);
              Blynk.virtualWrite(V16, callCount*3+MAXSET);
              Blynk.virtualWrite(V17, reco);
              Blynk.virtualWrite(V18, OAT);
            }
          #endif
        }
        else
        {
          if (verbose>2) Serial.printf("Particle not connected....connecting\n");
          Particle.connect();
          numTimeouts++;
        }
    }
    if (verbose>5) Serial.printf("end loop()\n");
}  // loop
