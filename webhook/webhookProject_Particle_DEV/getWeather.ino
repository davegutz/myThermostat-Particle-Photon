/* To setup your weather hook for US:
open Command Prompt
> particle webhook GET get_weather http://w1.weather.gov/xml/current_obs/KBVY.xml
KBVY is the US weather station designation you can find in w1.weather.gov using
a little drop-down menu they have

get_weather will be the name of my call.
particle knows my user auth id so this stuff will be available to me when I query

*/
SYSTEM_THREAD(ENABLED);       // Make sure heat system code always run regardless of network status
//#define TESTING_WEATHER                     // To continuously check, for debugging
#define WEATHER_WAIT      900UL             // Time to wait for webhook
#define READ_DELAY       5000UL             // Sensor read wait, ms
int                 verbose         = 3;    // Whatever you can tolerate on Serial
double              updateTime      = 0.0;  // Control law update time, sec

bool    weatherGood         = false;
int     badWeatherCall      = 0;
long    updateweatherhour   = 0;
double  tempf               = 30;   // OAT, deg F

// called once on startup
void setup()
{
  // For simplicity, we'll format our weather data as text, and pipe it to serial.
  // but you could just as easily display it in a webpage or pass the data to another system.

  // Learn more about the serial commands here http://docs.particle.io/firmware/#communication-serial
  // You can also watch what's sent over serial with the particle cli with
  //  particle serial monitor
  Serial.begin(9600);

  // Lets listen for the hook response
  Spark.subscribe("hook-response/get_weather", gotWeatherData, MY_DEVICES);

  // Lets give ourselves 10 seconds before we actually start the program.
  // That will just give us a chance to open the serial monitor before the program sends the request
  for(int i=0;i<5;i++)
  {
    Serial.println("waiting " + String(5-i) + " seconds before we publish");
    Serial.flush();
    delay(1000);
  }
  Serial.print("initializing weather...");
  Serial.flush();
  getWeather();
}



void loop()
{
  unsigned long           nowLoop = millis();     // Keep track of time
  bool                    read;                   // Read, T/F
  static unsigned long    lastRead     = nowLoop; // Last read time, ms
  read    = (nowLoop-lastRead)    >= READ_DELAY;
  if ( read     )
  {
    lastRead        = nowLoop;
    unsigned long           then = millis();     // Keep track of time
    getWeather();
    unsigned long           now = millis();     // Keep track of time
    updateTime    = float(now-then)/1000.0;
    if (verbose>0) Serial.printf("update=%f\n", updateTime);
  }


  // and wait at least 60 seconds before doing it again
  delay(30000);
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
