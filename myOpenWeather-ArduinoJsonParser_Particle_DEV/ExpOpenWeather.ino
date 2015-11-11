#include "openweathermap.h"
#include "HttpClient.h"

unsigned int nextTime = 0;    // next time to contact the server

Weather* weather;
HttpClient* httpClient;
// see http://openweathermap.org/appid
const   char      weathAuth[]	= "796fb85518f8b9eac4ad983306b3246c";
// id number  List of city ID city.list.json.gz can be downloaded here http://bulk.openweathermap.org/sample/
const 	int				location 		= 4954801; 	// Wenham, MA, US

void setup() {
	Serial.begin(9600);

	httpClient = new HttpClient();
	  weather  = new Weather(location, httpClient, weathAuth);
	  weather->setFahrenheit();
}

void loop() {
	if (nextTime > millis()) {
		// keep the same color while waiting
		return;
	}
	// print weather
	weather_response_t resp = weather->cachedUpdate();
	if (resp.isSuccess) {
		Serial.printf("temp_now=%f", resp.temp_now);
		Serial.println(resp.descr);
	}

	// check again in 30 seconds:
	nextTime = millis() + 30000;
}
