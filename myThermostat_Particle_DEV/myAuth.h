#ifndef myAuth_h
#define myAuth_h
#ifndef BARE_PHOTON
    const   String    blynkAuth  = "4f1de4949e4c4020882efd3e61bdd6cd"; // PhotonTHermo9HYK thermostat
#else
    const   String    blynkAuth  = "d2140898f7b94373a7868f158a3403a1"; // PhotonExp9HYK unconnected photon
#endif
const       String    weathAuth  = "796fb85518f8b9eac4ad983306b3246c";
const 	int			   location 	   = 4954801;  // id number  List of city ID city.list.json.gz can be downloaded here http://bulk.openweathermap.org/sample/
#endif
