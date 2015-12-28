#include "openweathermap.h"
#ifndef NO_WEATHER
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
	 "main":{"temp":42.44,"pressure":1041.89,"humidity":100,"temp_min":42.44,"temp_max":42.44,"sea_level":1043.71,"grnd_level":1041.89},
	 "wind":{"speed":8.14,"deg":268},"clouds":{"all":0},
	 "dt":1447058949,
	 "sys":{"message":0.0034,"country":"US","sunrise":1447068450,"sunset":1447104404},
	 "id":4954801,
	 "name":"Wenham",
	 "cod":200}

	 */

	unsigned char buffer[data.length()];
	data.getBytes(buffer, sizeof(buffer), 0);
	#ifndef WEATHER_BUG
		JsonHashTable root = Weather::parser.parseHashTable((char*) buffer);
		if (!root.success()) {
			Serial.printf("Parsing fail: could be an invalid JSON, or too many tokens, %s", (char*)buffer);
			return false;
		}
	#else
		char* badb = \
"{\"coord\":{\"lon\":-70.89,\"lat\":42.6},\
\"weather\":[{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50d\"},{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10d\"}],\
\"base\":\"cmc stations\",\
\"main\":{\"temp\":46.33,\"pressure\":1007,\"humidity\":93,\"temp_min\":42.01,\"temp_max\":54},\
\"wind\":{\"speed\":5.62,\"deg\":20},\"clouds\":{\"all\":90},\
\"dt\":1451249923,\
\"sys\":{\"type\":1,\"id\":1273,\"message\":0.0058,\"country\":\"US\",\"sunrise\":1451218368,\"sunset\":1451251036},\
\"id\":4954801,\
\"name\":\"Wenham\",\
\"cod\":200}";
		JsonHashTable root = Weather::parser.parseHashTable(badb);
		if (!root.success()) {
			Serial.printf("Parsing fail: could be an invalid JSON, or too many tokens, %s", badb);
			return false;
		}
	#endif
	response.gmt 				= root.getLong("dt");
	JsonHashTable main 	= root.getHashTable("main");
	response.temp_now 	= main.getDouble("temp");
	response.isSuccess	= true;
	return true;
}

/**
 * Reads from the cache if there is a fresh and valid response.
 */
weather_response_t Weather::cachedUpdate(int verbose) {
	if (lastsync == 0 || (lastsync + weather_sync_interval) < millis()) {
		weather_response_t resp;
		if(this->update(resp)){
			lastReponse = resp;
			lastsync = millis();
		}
	} else {
		if (verbose > 3)
		Serial.println("using cached weather");
	}
	return lastReponse;
}
#endif
