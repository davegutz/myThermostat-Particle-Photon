/* To setup your weather hook for US:
open Command Prompt
> particle webhook GET get_weather http://w1.weather.gov/xml/current_obs/KBVY.xml
KBVY is the US weather station designation you can find in w1.weather.gov using
a little drop-down menu they have

get_weather will be the name of my call.
particle knows my user auth id so this stuff will be available to me when I query

*/
bool    weatherGood         = false;
int     badWeatherCall      = 0;
long    updateweatherhour   = 0;

// called once on startup
void setup() {
    // For simplicity, we'll format our weather data as text, and pipe it to serial.
    // but you could just as easily display it in a webpage or pass the data to another system.

    // Learn more about the serial commands here http://docs.particle.io/firmware/#communication-serial
    // You can also watch what's sent over serial with the particle cli with
    //  particle serial monitor
    Serial.begin(115200);

    // Lets listen for the hook response
    Spark.subscribe("hook-response/get_weather", gotWeatherData, MY_DEVICES);

    // Lets give ourselves 10 seconds before we actually start the program.
    // That will just give us a chance to open the serial monitor before the program sends the request
    for(int i=0;i<10;i++) {
        Serial.println("waiting " + String(10-i) + " seconds before we publish");
        delay(1000);
    }
}


// called forever really fast
void loop() {

    // Let's request the weather, but no more than once every 60 seconds.
    //Serial.println("Requesting Weather!");

    //Spark.publish("get_weather");
    // publish the event that will trigger our Webhook
    if (Time.hour() != updateweatherhour)
    {
        Serial.println("Requesting Weather because out of date");
        getWeather();
    }
    else
    {
        Serial.println("Weather up to date");
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


    String str = String(data);
    String locationStr = tryExtractString(str, "<location>", "</location>");
    String weatherStr = tryExtractString(str, "<weather>", "</weather>");
    String tempStr = tryExtractString(str, "<temp_f>", "</temp_f>");
    String windStr = tryExtractString(str, "<wind_string>", "</wind_string>");

    if (locationStr != NULL) {
        Serial.println("At location: " + locationStr);
    }

    if (weatherStr != NULL) {
        Serial.println("The weather is: " + weatherStr);
    }

    if (tempStr != NULL) {
        Serial.println("The temp is: " + tempStr + String(" *F"));
        double tempf=atof(tempStr);
        Serial.printf("tempf=%f\n",tempf);
    }

    if (windStr != NULL) {
        Serial.println("The wind is: " + windStr);
    }
    weatherGood = true;
    updateweatherhour = Time.hour();
}

// Returns any text found between a start and end string inside 'str'
// example: startfooend  -> returns foo
String tryExtractString(String str, const char* start, const char* end) {
    if (str == NULL) {
        return NULL;
    }

    int idx = str.indexOf(start);
    if (idx < 0) {
        return NULL;
    }

    int endIdx = str.indexOf(end);
    if (endIdx < 0) {
        return NULL;
    }

    return str.substring(idx + strlen(start), endIdx);
}

//Updates Weather Forecast Data
void getWeather() {
  Serial.print("in getWeather");
  Serial.println();
  weatherGood = false;
  // publish the event that will trigger our webhook
  Spark.publish("get_weather");

  unsigned long wait = millis();
  //wait for subscribe to kick in or 5 secs
  while (!weatherGood && (millis() < wait + 5000UL))
    //Tells the core to check for incoming messages from particle cloud
    Spark.process();
  if (!weatherGood) {
    Serial.print("Weather update failed");
    Serial.println();
    badWeatherCall++;
    if (badWeatherCall > 2) {
      //If 3 webhook call fail in a row, Print fail
      Serial.print("Webhook Weathercall failed!");
    }
  }
  else
    badWeatherCall = 0;
}//End of getWeather function
