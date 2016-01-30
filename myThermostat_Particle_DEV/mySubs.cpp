#include "application.h"
#include "mySubs.h"
#include "math.h"

#ifndef NO_WEATHER_HOOK
  int                         badWeatherCall  = 0;  // webhook lookup counter
#endif
const   int                   EEPROM_ADDR     = 1;  // Flash address


extern  bool                  held;                 // Web toggled permanent and acknowledged
extern  int                   set;                  // Selected sched, F
extern  double                tempf;                // webhook OAT, deg F
extern  double                Ta_Obs;               // Modeled air temp, F
extern  int verbose;
#ifndef NO_WEATHER_HOOK
#endif
#ifndef NO_WEATHER_HOOK
  extern long                 updateweatherhour;    // Last hour weather updated
  extern bool                 weatherGood;          // webhook OAT lookup successful, T/F
#endif
extern  int                   webDmd;               // Web sched, F
extern bool                   webHold;              // Web permanence request

extern float hourCh[7][NCH];
extern const float tempCh[7][NCH];

// Convert time to decimal for easy lookup
double decimalTime(unsigned long *currentTime, char* tempStr)
{
//    *currentTime = rtc.now();
    Time.zone(GMT);
    *currentTime = Time.now();
    #ifndef FAKETIME
        uint8_t dayOfWeek = Time.weekday(*currentTime)-1;  // 0-6
        uint8_t hours     = Time.hour(*currentTime);
        uint8_t minutes   = Time.minute(*currentTime);
        uint8_t seconds   = Time.second(*currentTime);
        if (verbose>3) Serial.printf("DAY %u HOURS %u\n", dayOfWeek, hours);
    #else
        // Rapid time passage simulation to test schedule functions
        uint8_t dayOfWeek = (Time.weekday(*currentTime)-1)*7/6;// minutes = days
        uint8_t hours     = Time.hour(*currentTime)*24/60; // seconds = hours
        uint8_t minutes   = 0; // forget minutes
        uint8_t seconds   = 0; // forget seconds

    #endif
    sprintf(tempStr, "%02u:%02u", hours, minutes);
    return (float(dayOfWeek)*24.0 + float(hours) + float(minutes)/60.0 + \
                        float(seconds)/3600.0);  // 0-6 days and 0 is Sunday
}




//Updates Weather Forecast Data
#ifndef NO_WEATHER_HOOK
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

// This function will get called when weather data comes in
#ifndef NO_WEATHER_HOOK
void gotWeatherData(const char *name, const char *data)
{
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
#endif

// Observer rate filter:  produces clean temperature rate signal from noisey measurement
// using an embedded house model.  Linear analysis may be needed to design coefficients.
// Simulink model provided in documentation.
double  houseTrack(const bool RESET, const double duty, const double Ta_Sense,\
   const double Ta_Obs, const double T)
{
    const   double  Kei       = 2e-4;                 // Rejection PI integrator gain, (r/s)/F
    const   double  Kep       = 1;                    // Rejection PI proportional gain, F/F
    static  double  intE      = 0.0;                  // Rejection integrator state, F
    double          rejectHeat;                       // Calculated duty to control house, 0-1
    double          ePI;                              // Rejection PI error, F
    double          Duty_Reject;                      // PI result, units 200*F?

    // Initialize
    if ( RESET )  intE = 0.0;

    // Rejection
    ePI         = Ta_Sense - Ta_Obs;
    intE        = max(min(intE + Kei*ePI*T, 1), -1);
    Duty_Reject = intE + Kep*ePI;
    rejectHeat  = max(min(Duty_Reject, 1), -1)*0.005;

    if ( verbose > 3) Serial.printf("Ta_Obs=%f, Ta_Sense=%f, intE=%f, Duty_Reject=%f, duty=%f, rejectHeat=%f\n",\
                      Ta_Obs, Ta_Sense, intE, Duty_Reject, duty, rejectHeat);

    return rejectHeat;
}


// HouseHeat Class Functions
// Constructors
HouseHeat::HouseHeat()
  :   name_(""), Ha_(0), Hc_(0), Hf_(0), Ho_(0), Rn_(0), Rx_(0), Tn_(0), Tx_(0)
{}
HouseHeat::HouseHeat(const String name, const double Ha, const double Hc, const double Hf, const double Ho, \
  const double Rn, const double Rx, const double Tn, const double Tx)
  :   name_(name), Ha_(Ha), Hc_(Hc), Hf_(Hf), Ho_(Ho), Rn_(Rn), Rx_(Rx), Tn_(Tn), Tx_(Tx), sNoise_(0)
{}
HouseHeat::HouseHeat(const String name, const double Ha, const double Hc, const double Hf, const double Ho, \
    const double Rn, const double Rx, const double Tn, const double Tx, const double sNoise)
    :   name_(name), Ha_(Ha), Hc_(Hc), Hf_(Hf), Ho_(Ho), Rn_(Rn), Rx_(Rx), Tn_(Tn), Tx_(Tx), sNoise_(sNoise)
  {}
// Calculate
double HouseHeat::update(const bool RESET, const double T, const double temp, const double duty, \
  const double otherHeat, const double OAT)
{
  // Three-state thermal model
  // The boiler in the house this was tuned to has a water reset schedule
  // that is a function of OAT.   If yours in constant, just
  // Tx to same value as Tn
  double Tb   = max(min((OAT-Rn_)/(Rx_-Rn_)*(Tx_-Tn_)+Tn_, Tn_), Tx_); // Curve interpolation
  // States
  if ( RESET )
  {
    Ta_   = temp;                           // Air temp in house, F
    Tw_   = (OAT*Ho_+Ta_*Ha_)/(Ho_+Ha_);    // Outside wall temp, F
    Tc_   = (Ta_*(Ha_+Hc_)-Tw_*Ha_)/Hc_;    // Core heater temp, F
  }
  // Derivatives
  double dTw_dt   = -(Tw_-Ta_)*Ha_ - (Tw_-OAT)*Ho_;
  double dTa_dt   = -(Ta_-Tw_)*Ha_ - (Ta_-Tc_)*Hc_;
  double dTc_dt   = -(Tc_-Ta_)*Hc_ + duty*(Tb-Tc_)*Hf_ + otherHeat;
  if ( verbose > 3 )
  {
    Serial.printf("%s:  dTw_dt=%7.3f, dTa_dt=%7.3f, dTc_dt=%7.3f\n", name_.c_str(), dTw_dt, dTa_dt, dTc_dt);
    Serial.printf("%s:  Tb=%7.3f, Tc=%7.3f, Ta=%7.3f, Tw=%7.3f, OAT=%7.3f\n", name_.c_str(), Tb, Tc_, Ta_, Tw_, OAT);
  }
  // Integration (Euler Backward Difference)
  Tw_  = min(max(Tw_+dTw_dt*T,  -40), 120);
  Ta_  = min(max(Ta_+dTa_dt*T,  -40), 120);
  Tc_  = min(max(Tc_+dTc_dt*T,  -40), 120);
  Ta_Sense_ = Ta_ + sNoise_ * float(random(-10, 10))/10.0;
  return dTa_dt;
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
    #ifndef BARE_PHOTON
    Ta_Obs  = values[3];

    #else
    Ta_Obs  = 68;
    #endif
    if ( (Ta_Obs  > MAXSET+1) | (Ta_Obs  < MINSET-1) ) Ta_Obs  = (MAXSET+MINSET)/2;
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


// Save temperature setpoint to flash for next startup.   During power
// failures the thermostat will reset to the condition it was in before
// the power failure.   Filter initialized to sensed temperature (lose anticipation briefly
// following recovery from power failure).
void saveTemperature()
{
    uint8_t values[4] = { (uint8_t)set, (uint8_t)held, (uint8_t)webDmd, (uint8_t)(roundf(Ta_Obs)) };
    EEPROM.put(EEPROM_ADDR, values);
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
    if (verbose>3) Serial.printf("hour=%f, recoTime=%f, tempSchd=%f, tempSchdShift=%f\n",\
                                            hourDecimal, recoTime, tempSchd, tempSchdShift);
    *reco               = tempSchdShift>tempSchd;
    tempSchd            = max(tempSchd, tempSchdShift); // Turn on early but not turn off early
    return tempSchd;
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


#ifndef NO_WEATHER_HOOK
// Returns any text found between a start and end string inside 'str'
// example: startfooend  -> returns foo
String tryExtractString(String str, const char* start, const char* end)
{
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
#endif
