# cronoduino
   Display the date and time from a DS3232 RTC 
   for one minute.
   Check the temperature against the weekly program table once per
   minute. (The DS3232 does a temperature conversion once every 64
   seconds)
   Temporary increment or decrement of the temperature (for one hour)
   with two buttons. When buttons are pressed the display shows the
   current temperature setup.
                                                                       
   Set the date and time by entering the following on the Arduino
   serial monitor:
      year,month,day,hour,minute,second,
                                                                       
   Where
      year can be two or four digits,
      month is 1-12,
      day is 1-31,
      hour is 0-23, and
      minute and second are 0-59.
                                                                      
   Entering the final comma delimiter (after "second") will avoid a
   one-second timeout and will allow the RTC to be set more accurately.
                                                                      
   Validity checking is done, invalid values or incomplete syntax
   in the input will not result in an incorrect RTC setting.
