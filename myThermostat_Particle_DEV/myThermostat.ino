/* myThermostat.ino
  Control home heating (not cooling) with a potentiometer, a web interface
  (Blynk), and by schedules.  This is written for a Particle Photon
  target.  The electrical schematic is the same as the Spark Nest Thermostat
  project with I2C 4.7K pullup resistors added and the solid state relay swapped out for
  a Reed relay to control a Triangle Tube Prestige furnace interface (Honeywell
  dial thermostat compatible).

  11-Nov-2015   Dave Gutz   Created

  TODO:::::::::::::::::::
  - WEATHER_BUG is turned on to illustrate memory leak that occurs when running with
  WEATHER in openweathermap.  Rapid flashing cyan over time.   Stress test with
  QUERY_DELAY = 100UL will reveal this quickly.


  Requirements:
  1.  Read temperature.
  2.  Determine setpoint and turn on furnace relay when temperature < setpoint.  Turn off relay when temperature
      reaches setpoint.
  3.  Read potentiometer POT and set a demand based on the reading.
  4.  Read Blynk Web demanded temperature WEB and set a demand based on the reading.
  5.  Read Blynk Web over-ride and disable the schedule and web demand functions as long as latched.
  6.  Schedule 4 changes in all 7 days of the week.   When time reaches a change
      table time instantly change demand to the change table temperature and hold.
  7.  Potentiometer is physically present and must always win if it is most recent change of
      setpoint.
  8.  The Blynk Web is needed to set temperature remotely for some reason and must win over the
      schedule but only if demanded so.
  9.  Following power loss the functions must return to the exact settings.
  10. Automatically change with daylight savings (this is built into SparkTime as default)
  11. A new device must come alive with CALL off.
  12. Logic should wait 5 seconds before turning furnace on or off, to help those with fat fingers
      to correct their keystrokes / sliding.
  13. Logic should not attempt to run more than one function in same pass.
  14. RECO function
      o Shift schedule early depending on OAT
      o No shift for OAT>40 F
      o 1 hr shift for OAT<10 F
      o linear in between and extrapolate colder
      o No shift for turning off furnace
      o Boolean indicator on Blynk
  15. Temperature compensation
      o Shift temperature to anticipate based on rate
      o Filter time constant for rate picked to be 1/10 of observed home constant
      o Gain picked to produce tempComp that is equal to observed overshoot

  Nomenclature (on Blynk):
   CALL Call for heat, boolean
   DMD  Temperature setpoint demanded by web, F
   HELD Confirmation of web HOLD demand, boolean
   HOLD Web HOLD demand, boolean
   HOUR Time being used by this program for troubleshooting, hours
   HUM  Measured humidity, %
   OAT  Outside air temperature, F
   POT  The pot reading converted to degrees demand, F
   RECO Recovery to warmer schedule on cold day underway, boolean
   SCHD The time-scheduled setpoint stored in tables, F
   SET  Temperature setpoint of thermostat, F
   T    Control law update time, sec
   TEMP Measured temperature, F
   TMPC Filtered anticipation temperature, F
   WEB  The Web temperature demand, F

   On your Blynk app:
   0.   Connect a green LED or 0-1 500 ms gage to V0 (CALL)
   1.   Connect a green history graph to V1 (SET)
   2.   Connect a red history graph (same as 1) to V2 (TEMP)
   3.   Connect a blue numerical 0-100 8 sec display to V3 (HUM)
   4i.  Connect a green 50-72 small slider to V4 OUT (DMD)
   4.   Connect an orange history graph to V4 (TMPC)
   5.   Connect a white numerical 0-1 500 ms display to V5 (HELD)
   6.   Connect a white switch to V6 (HOLD)
   7.   Connect a purple numerical 0-200 5 sec display to V15 (TIME)
   8.   Connect a purple numerical 0-200 1 sec display to V8 (T)
   9.   Connect an orange numerical 50-72 1 sec display to V9 (POT)
   10.  Connect an orange numerical 50-72 1 sec display to V10 (WEB)
   11.  Connect a green numerical 50-72 1 sec display to V11 (SET)
   12.  Connect an orange numerical 50-72 10 sec display to V12 (SCHD)
   13.  Connect a red numerical 30-100 1 sec display to V13 (TEMP)
   14.  Connect a blue history graph to V16 (CALL)
   15.  Connect an orange numerical 0-1 5 sec display to V17 (RECO)
   16.  Connect a blue numerical -50 - 120 30 sec display to V18 (OAT)

   Dependencies:  ADAFRUIT-LED-BACKPACK, SPARKTIME, SPARKINTERVALTIMER, BLYNK,
   blynk app account, Particle account
*/

//Sometimes useful for debugging
//#pragma SPARK_NO_PREPROCESSOR
//
// Standard
#include "application.h"                    // For Particle code classes
SYSTEM_THREAD(ENABLED);                     // Make sure heat system code always run regardless of network status
//
// Disable flags if needed, usually commented
//#define BARE_PHOTON                       // Run bare photon for testing.  Bare photon without this goes dark or hangs trying to write to I2C
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
#define WEATHER_WAIT      900UL             // Time to wait for weather webhook, ms
#define BLYNK_TIMEOUT_MS 2000UL             // Network timeout in ms;  default provided in BlynkProtocol.h is 2000
#define CONTROL_DELAY    1000UL             // Control law wait (1000), ms
#define PUBLISH_DELAY    10000UL            // Time between cloud updates (10000), ms
#define READ_DELAY       5000UL             // Sensor read wait (5000, 100 for stress test), ms
#define QUERY_DELAY      15000UL            // Web query wait (15000, 100 for stress test), ms
#define DIM_DELAY        3000UL             // LED display timeout to dim, ms
#define DISPLAY_DELAY    300UL              // LED display scheduling frame time, ms
#ifndef BARE_PHOTON
  #define FILTER_DELAY   5000UL             // In range of tau/4 - tau/3  * 1000, ms
#else
  #define FILTER_DELAY   3500UL             // In range of tau/4 - tau/3  * 1000, ms
#endif
#define HEAT_PIN    A1                      // Heat relay output pin on Photon (A1)
#define HYST        0.5                     // Heat control law hysteresis (0.5), F
#define POT_PIN     A2                      // Potentiometer input pin on Photon (A2)
#define TEMP_SENSOR 0x27                    // Temp sensor bus address (0x27)
#define TEMPCAL     -4                      // Calibrate temp sense (0), F
#define MINSET      50                      // Minimum setpoint allowed (50), F
#define MAXSET      72                      // Maximum setpoint allowed (72), F
#define NCH         4                       // Number of temp changes in daily sched (4)
#define ONE_DAY_MILLIS (24*60*60*1000)      // Number of milliseconds in one day (24*60*60*1000)
//
// Dependent includes.   Easier to debug code if remove unused include files
#include "SparkIntervalTimer.h"
#include "SparkTime.h"
#ifndef NO_BLYNK
  #include "blynk.h"
  #include "BlynkHandlers.h"
#endif
#include <math.h>
#include "adafruit-led-backpack.h"
#include "pixmaps.h"
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
#ifndef NO_WEATHER_HOOK
  int               badWeatherCall  = 0;    // webhook lookup counter
#endif
bool                call            = false;// Heat demand to relay control
double              callCount;              // Floating point of bool call for calculation
#ifndef BARE_PHOTON
  double            compGain        = 100.; // Temperature compensation gain, deg/(deg/sec)
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
SparkTime           rtc;                    // Time value
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
double              tempf         = 30;     // webhook OAT, deg F
double              Thouse;                 // House bulk temp, F
UDP                 UDPClient;              // Time structure
double              updateTime      = 0.0;  // Control law update time, sec
#ifndef NO_WEATHER_HOOK
  long              updateweatherhour= 0;   // Last hour weather updated
#endif
static const int    verbose         = 3;    // Debug, as much as you can tolerate
#ifndef NO_WEATHER_HOOK
  bool              weatherGood     = false;// webhook OAT lookup successful, T/F
#endif
int                 webDmd          = 62;   // Web sched, F
bool                webHold         = false;// Web permanence request

// Schedules
bool hourChErr = false;
// Time to trigger setting change
static float hourCh[7][NCH] = {
    6, 8, 16, 22,   // Sun
    4, 7, 16, 22,   // Mon
    4, 7, 16, 22,   // Tue
    4, 7, 16, 22,   // Wed
    4, 7, 16, 22,   // Thu
    4, 7, 16, 22,   // Fri
    6, 8, 16, 22    // Sat
};
// Temp setting after change in above table.   Change holds until next
// time trigger.  Temporarily over-ridden either by pot or web adjustments.
static const float tempCh[7][NCH] = {
    68, 62, 68, 62, // Sun
    68, 62, 68, 62, // Mon
    68, 62, 68, 62, // Tue
    68, 62, 68, 62, // Wed
    68, 62, 68, 62, // Thu
    68, 62, 68, 62, // Fri
    68, 62, 68, 62  // Sat
};

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

// Simple embedded house model for testing logic
double modelTemperature(bool call, double OAT, double T)
{
    static const    double Chouse     = 1000;   // House thermal mass, BTU/F
    static const    double Qfurnace   = 40000;  // Furnace output, BTU/hr
    static const    double Hhouse     = 400;    // House loss, BTU/hr/F
    double dQ   = float(call)*Qfurnace - Hhouse*(Thouse-OAT);   // BTU/hr
    double dTdt = dQ/Chouse/3600.0;             // House rate of change, F/sec
    Thouse      += dTdt*float(FILTER_DELAY)/1000.0;
    Thouse      = min(max(Thouse, 40), 90);
    return(Thouse);
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

// Setup LEDs
void setupMatrix(Adafruit_8x8matrix m)
{
    m.clear();
    m.writeDisplay();
    m.setTextSize(1);
    m.setTextWrap(false);
    m.setTextColor(LED_ON);
    m.setRotation(0);
    m.setCursor(0, 0);
}


// Convert time to decimal for easy lookup
double decimalTime(unsigned long *currentTime, char* tempStr)
{
    *currentTime = rtc.now();
    #ifndef FAKETIME
        uint8_t dayOfWeek = rtc.dayOfWeek(*currentTime);
        uint8_t hours     = rtc.hour(*currentTime);
        uint8_t minutes   = rtc.minute(*currentTime);
        uint8_t seconds   = rtc.second(*currentTime);
    #else
        // Rapid time passage simulation to test schedule functions
        uint8_t dayOfWeek = rtc.minute(*currentTime)*7/6;      // minutes = days
        uint8_t hours     = rtc.second(*currentTime)*24/60;    // seconds = hours
        uint8_t minutes   = 0;                                // forget minutes
        uint8_t seconds   = 0;                                // forget seconds
    #endif
    sprintf(tempStr, "%02u:%02u", hours, minutes);
    return (float(dayOfWeek)*24.0 + float(hours) + float(minutes)/60.0 + \
                        float(seconds)/3600.0);  // 0-6 days and 0 is Sunday
}

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

// Calculate recovery time to heat better on cold days
double recoveryTime(double OAT)
{
    double recoTime = (40.0 - OAT)/30.0;
    recoTime        = max(min(recoTime, 2.0), 0.0);
    return recoTime;
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

#ifndef NO_WEATHER_HOOK
// Returns any text found between a start and end string inside 'str'
// example: startfooend  -> returns foo
String tryExtractString(String str, const char* start, const char* end) {
  if (str == NULL)
  {
    return NULL;
  }
  int idx = str.indexOf(start);
  if (idx < 0)
  {
    return NULL;
  }
  int endIdx = str.indexOf(end);
  if (endIdx < 0)
  {
    return NULL;
  }
  return str.substring(idx + strlen(start), endIdx);
}


// This function will get called when weather data comes in
void gotWeatherData(const char *name, const char *data) {
  // Important note!  -- Right now the response comes in 512 byte chunks.
  //  This code assumes we're getting the response in large chunks, and this
  //  assumption breaks down if a line happens to be split across response chunks.
  //
  // Sample data:
  //  <location>Minneapolis, Minneapolis-St. Paul International Airport, MN</location>
  //  <weather>Overcast</weather>
  //  <temperature_string>26.0 F (-3.3 C)</temperature_string>
  //  <temp_f>26.0</temp_f>

  String str          = String(data);
  String locationStr  = tryExtractString(str, "<location>",     "</location>");
  String weatherStr   = tryExtractString(str, "<weather>",      "</weather>");
  String tempStr      = tryExtractString(str, "<temp_f>",       "</temp_f>");
  String windStr      = tryExtractString(str, "<wind_string>",  "</wind_string>");

  if (locationStr != NULL && verbose>3) {
    if(verbose>3) Serial.println("");
    Serial.println("At location: " + locationStr);
  }
  if (weatherStr != NULL && verbose>3) {
    Serial.println("The weather is: " + weatherStr);
  }
  if (tempStr != NULL) {
    weatherGood = true;
    #ifndef TESTING_WEATHER
      updateweatherhour = Time.hour();  // To check once per hour
    #endif
    tempf = atof(tempStr);
    if (verbose>2)
    {
      if (verbose<4) Serial.println("");
      Serial.println("The temp is: " + tempStr + String(" *F"));
      Serial.flush();
      Serial.printf("tempf=%f\n", tempf);
      Serial.flush();
    }
  }
  if (windStr != NULL && verbose>3) {
    Serial.println("The wind is: " + windStr);
  }
}

//Updates Weather Forecast Data
void getWeather()
{
  // Don't check if same hour
  if (Time.hour() == updateweatherhour)
  {
    if (verbose>2 && weatherGood) Serial.printf("Weather up to date, tempf=%f\n", tempf);
    return;
  }

  if (verbose>2)
  {
    Serial.print("Requesting Weather from webhook...");
    Serial.flush();
  }
  weatherGood = false;
  // publish the event that will trigger our webhook
  Spark.publish("get_weather");

  unsigned long wait = millis();
  //wait for subscribe to kick in or 0.9 secs
  while (!weatherGood && (millis() < wait + WEATHER_WAIT))
  {
    //Tells the core to check for incoming messages from partile cloud
    Spark.process();
    delay(50);
  }
  if (!weatherGood)
  {
    if (verbose>3) Serial.print("Weather update failed.  ");
    badWeatherCall++;
    if (badWeatherCall > 2)
    {
      //If 3 webhook calls fail in a row, Print fail
      if (verbose>0) Serial.println("Webhook Weathercall failed!");
      badWeatherCall = 0;
    }
  }
  else
  {
    badWeatherCall = 0;
  }
} //End of getWeather function
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
  rtc.begin(&UDPClient, "pool.ntp.org");  // Workaround - see particle.io
  rtc.setTimeZone(-5); // gmt offset

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
