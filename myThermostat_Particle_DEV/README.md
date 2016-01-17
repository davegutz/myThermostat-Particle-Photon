 myThermostat.ino
  Control home heating (not cooling) with a potentiometer, a web interface
  (Blynk), and by schedules.  This is written for a Particle Photon
  target.  The electrical schematic is the same as the Spark Nest Thermostat
  project with I2C 4.7K pullup resistors added and the solid state relay swapped out for
  a Reed relay to control a Triangle Tube Prestige furnace interface (Honeywell
  dial thermostat compatible).

  11-Nov-2015   Dave Gutz   Created
  17-Jan-2016   Dave Gutz   Major cleanup, rtc eliminated and files organized

  TODO:::::::::::::::::::

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
  16. GMT shift
      o Use the variable GMT define statement to set your difference to GMT in hours.

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
