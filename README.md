# myThermostat-Particle-Photon
A thermostat app that works in a wall-mounted Photon, responds to Blynk web commands, has a built-in 
programmable schedule, and displays status on Blynk.  It heats only and easily modifiable to cool as
well will have to figure out how to integrate as a system with the hardware cooling.  Suggestions 
to keep the installation safe are provided.    There is an embedded heating system model to allow you 
to test code changes on a bare Photon using the web interface, including the Blynk interface.   This
bare option lights the blue LED on a call for heat.


README.txt   File for Thermostat gitHub project
11-Nov-2015 	Dave Gutz 	Created

Quickstart:
To download, click the DOWNLOAD ZIP button.
Open the myThermostat_Particle_DEV folder in Particle-DEV.
It will run on a bare Photon using a modeled system without I2C networking.  (#undef HAVEI2C).  This will post to Blynk and respond as well.
You will need accounts at Particle.io and Blynk.cc and openweathermap.org.   The Blynk instructions are in the header of the
.ino file.

Notes:
-  	Heating only application (New England).

-  	My Prestige furnace required a Reed relay instead of the SSR used by Spark project.

-  	Modified the Spark wiring diagram for relay.   See the "Reed SIP Version" sheet of 
   	the PhotonThermostatLayout document in schematics folder.

-  	Part numbers are listed on the layout.   I bought stuff at Mouser about $100.

-  	I also had to buy a bunch of solder supplies at about $200

-	To protect the house, I wired this device in series with existing thermostat that is
	then set at about 72 F.

-  	Alternatively, one could wire up a couple thermal switches to protect house:  
	one in series that opens at 80F (I used old thermostat) and
	another in parallel that closes at 50 F.  ( I left this off and installed manual bypass switch)/

-  	Pull up resistors shown in the PhotonThermostatLayout, but not in the Spark schematic,
 	ultimately were not needed.  I thought they were to fix problems ultimately traced to 
   	bad code (not trasmitting to I2C properly).

-  	For DEV use, only one folder used for project and must contain only *.cpp, *.h, *.ino files 
   	and only one .ino app.

-  	I borrowed from two other developers, tweaked their stuff, and put in with the blob of
	code Particle-DEV needs:
		ArduinoJsonParser-template:  app with a slim version of the Json parser that uses templates.
			- 	this was needed to work with openweather-spark-lib-master code that for some
				reason did not work without it
		openweather-spark-lib-master  app to query open weather for outside air temperature

-  	I needed to copy from various repositories
		HttpClient.*
		blynk.* 
		adafruit-led-backpack.*
		SparkTime.*

-  	I needed to set up accounts at:
		Particle.io
		Blynk.cc
		openweathermap.org

-  	Downloads:
		Particle-DEV 1.0.19
		Eagle 7.2.0 to view electrical schematics
	