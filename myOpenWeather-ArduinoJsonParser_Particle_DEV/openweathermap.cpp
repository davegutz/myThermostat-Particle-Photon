#include "openweathermap.h"
/*
#include <cstdio>
#include <limits>
#include <cstdint>
#include <cinttypes>
*/

/** Show temp in degree celsius */
void Weather::setCelsius() {
	this->unitsForTemperature = "metric";
}
void Weather::setFahrenheit() {
	this->unitsForTemperature = "imperial";
}

Weather::Weather(int location, HttpClient* client, String apiKey) {
	this->location = location;
	this->client = client;
	this->apiKey = apiKey;

	// default:
	setCelsius();

	// init cache
	this->lastsync = 0;
	this->weather_sync_interval = 1000 * 3600; // 1 hr in milliseconds
}

bool Weather::update(weather_response_t& response) {
	request.hostname = "api.openweathermap.org";
	request.port = 80;
	request.path = "/data/2.5/weather?id=" //
	+ location // id number  List of city ID city.list.json.gz can be downloaded here http://bulk.openweathermap.org/sample/
			+ "&units=" + unitsForTemperature // metric or imperial
			+ "&mode=json" 										// xml or json
			+ "&APPID=" + apiKey; 						// see http://openweathermap.org/appid
	request.body = "";
	http_response_t http_response;
	this->client->get(request, http_response);
	if (http_response.status == 200) {
		return parse(http_response.body, response);
	} else {
		Serial.print("weather request failed ");
		return false;
	}
}

bool Weather::parse(String& data, weather_response_t& response) {
	/*
	 * example:

	 {"coord":{"lon":-70.89,"lat":42.6},
	 "weather":[{"id":800,"main":"Clear","description":"Sky is Clear","icon":"01n"}],
	 "base":"cmc stations",
	 "main":{"temp":47.79,"pressure":1039.14,"humidity":89,"temp_min":47.79,"temp_max":47.79,"sea_level":1041.02,"grnd_level":1039.14},
	 "wind":{"speed":8.1,"deg":285},
	 "clouds":{"all":0},
	 "dt":1447031754,
	 "sys":{"message":0.0073,"country":"US","sunrise":1447068426,"sunset":1447104425},
	 "id":4954801,"name":"Wenham",
	 "cod":200}

	 */

	unsigned char buffer[data.length()];
	data.getBytes(buffer, sizeof(buffer), 0);
	JsonHashTable root = Weather::parser.parseHashTable((char*) buffer);
	if (!root.success()) {
		Serial.println("Parsing fail: could be an invalid JSON, or too many tokens");
		return false;
	}
	JsonHashTable main = root.getHashTable("main");
	response.temp_now = main.getDouble("temp");

	//  response.temp_now = main.getDoubleFromToken("temp");
	response.isSuccess= true;
	return true;
}

/**
 * Reads from the cache if there is a fresh and valid response.
 */
weather_response_t Weather::cachedUpdate() {
	if (lastsync == 0 || (lastsync + weather_sync_interval) < millis()) {
		weather_response_t resp;
		if(this->update(resp)){
			lastReponse = resp;
			lastsync = millis();
		}
	} else {
		Serial.println("using cached weather");
	}
	return lastReponse;
}
