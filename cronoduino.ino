/*----------------------------------------------------------------------*
   Display the date and time from a DS3231 or DS3232 RTC every second
   for one minute.
   Check the temperature against the weekly program table once per
   minute. (The DS3231 does a temperature conversion once every 64
   seconds. This is also the  default for the DS3232.)
   Temporary increment or decrement of the temperature (for one hour)
   with two buttons. When buttons are pressed the display shows the
   current temperature setup.
 *                                                                      *
   Set the date and time by entering the following on the Arduino
   serial monitor:
      year,month,day,hour,minute,second,
 *                                                                      *
   Where
      year can be two or four digits,
      month is 1-12,
      day is 1-31,
      hour is 0-23, and
      minute and second are 0-59.
 *                                                                      *
   Entering the final comma delimiter (after "second") will avoid a
   one-second timeout and will allow the RTC to be set more accurately.
 *                                                                      *
   Validity checking is done, invalid values or incomplete syntax
   in the input will not result in an incorrect RTC setting.
 *                                                                      *
 *                                                                      *
   This work is licensed under the Creative Commons Attribution-
   ShareAlike 3.0 Unported License. To view a copy of this license,
   visit http://creativecommons.org/licenses/by-sa/3.0/ or send a
   letter to Creative Commons, 171 Second Street, Suite 300,
   San Francisco, California, 94105, USA.
  ----------------------------------------------------------------------*/

#include <Streaming.h>
#include <Time.h>

#include <SPI.h>
#include <Wire.h>             //http://arduino.cc/en/Reference/Wire
#include <DS3232RTC.h>        //http://github.com/JChristensen/DS3232RTC
#include <dht.h>
#include <avr/pgmspace.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Timezone.h>         //https://github.com/JChristensen/Timezone

// Central Europe Time Zone (Rome)
TimeChangeRule myCEST = {"CEST", Last, Sun, Mar, 2, +120};    //Daylight time = UTC +2 hours
TimeChangeRule myCET = {"CET", Last, Sun, Oct, 3, +60};       //Standard time = UTC +1 hours
Timezone myTZ(myCEST, myCET);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev

 

// If using software SPI (the default case):
#define OLED_D1   8  //MOSI
#define OLED_D0   9  //CLK
#define OLED_DC    6
#define OLED_CS    5
#define OLED_RESET 7
Adafruit_SSD1306 display(OLED_D1, OLED_D0, OLED_DC, OLED_RESET, OLED_CS);


#define DELTAY 2
#define BOUNCE_DURATION 200   // define an appropriate bounce time in ms for switches

volatile unsigned long bounceTime1 = 0; // variable to hold ms count to debounce a pressed switch
volatile unsigned long bounceTime2 = 0; // variable to hold ms count to debounce a pressed switch


#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16


#if (SSD1306_LCDHEIGHT != 64)
//#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

dht DHT11;


#define TEMP_OFFSET -3

#define HUM_OFFSET 10

#define DHT11PIN 4

#define MINTEMP 10 //minimum temperature that can be setup

#define MAXTEMP 27 //maximum temperature that can be setup

#define ERRORDISPLAYDELAY 2000 // number of ms for showing an error on the OLED display

#define ERRORONTIMESET 10 // Error on time setup

#define RELEONPIN 11    // relay coil 2: switch on

#define RELEOFFPIN 10   // relay coil 1: switch off

/* temperature time table. Temperature are multiplied by 10 */

const uint16_t weektemp[7][24] PROGMEM =
{ //00   01   02   03   04   05   06   07   08   09   10   11   12   13   14   15   16   17   18   19   20   21   22   23
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 150, 150, 150, 210, 210, 210, 210, 150, 150}, //sun
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 210, 150, 150, 210, 210, 210, 210, 150, 150}, //mon
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 210, 150, 150, 210, 210, 210, 210, 150, 150}, //tue
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 210, 150, 210, 210, 210, 210, 210, 150, 150}, //wed
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 210, 150, 150, 210, 210, 210, 210, 150, 150}, //thu
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 210, 150, 150, 210, 210, 210, 210, 150, 150}, //fri
  { 150, 150, 150, 150, 150, 180, 210, 210, 210, 150, 150, 150, 150, 150, 150, 150, 150, 150, 210, 210, 210, 210, 150, 150}  //sat
};

//{  0,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230}, //mon


word setuptemp;

float RTCTemp;
float DHT11Temp;
float DHT11Hum;


volatile time_t t;
volatile time_t tLast;
volatile time_t tLast2;
volatile time_t LastChanged=0;
volatile time_t tLast3;
volatile time_t tLast4;
volatile time_t tLast5;

volatile int displaytype = 3;

boolean running = false; // heating system runnung flag
boolean fx = false; // ":" character toggling each second flag

void parseserial()
{
    //note that the tmElements_t Year member is an offset from 1970,
    //but the RTC wants the last two digits of the calendar year.
    //use the convenience macros from Time.h to do the conversions.
    tmElements_t tm;
    int y = Serial.parseInt();
    if (y >= 100 && y < 1000)
      Serial << F("Error: Year must be two digits or four digits!") << endl;
    else {
      if (y >= 1000)
        tm.Year = CalendarYrToTm(y);
      else    //(y < 100)
        tm.Year = y2kYearToTm(y);
      tm.Month = Serial.parseInt();
      tm.Day = Serial.parseInt();
      tm.Hour = Serial.parseInt();
      tm.Minute = Serial.parseInt();
      tm.Second = Serial.parseInt();
      if ( (tm.Month >= 1 && tm.Month <= 12) &&
           (tm.Day >= 1  && tm.Day <= 31) &&
           (tm.Hour >= 0 && tm.Hour <= 24) &&
           (tm.Minute >= 0 && tm.Minute <= 59) &&
           (tm.Second >= 0 && tm.Second <= 59)) {
        time_t t_local = makeTime(tm);
        t=myTZ.toUTC(t_local);
        RTC.set(t);        //use the time_t value to ensure correct weekday is set
        setTime(t);
        Serial << F("RTC set to: ");
        printDateTime(t);
        Serial << endl;
      }
      else {
        Serial << F("Invalid Date/Time: ");
        Serial << y << F(" ");
        Serial << tm.Month << F(" ");
        Serial << tm.Day << F(" ");
        Serial << tm.Hour << F(" ");
        Serial << tm.Minute << F(" ");
        Serial << tm.Second << endl;
      }
      //dump any extraneous input
      while (Serial.available() > 0) Serial.read();
    }
  }

void setup(void)
{
  //initialize serial
  Serial.begin(115200);
  // Initialize OLED display: by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);

  display.clearDisplay();
  display.setTextColor(WHITE);
  // init done
  attachInterrupt(0, dectemp, FALLING);
  attachInterrupt(1, inctemp, FALLING);

  pinMode(RELEOFFPIN, OUTPUT); // relay coil 1: switch off
  pinMode(RELEONPIN, OUTPUT); // relay coil 2: switch on
  //pinMode(12, OUTPUT);
  //setSyncProvider() causes the Time library to synchronize with the
  //external RTC by calling RTC.get() every five minutes by default.
  setSyncProvider(RTC.get);
  Serial << F("RTC Sync");
  if (timeStatus() != timeSet) {
    Serial << F(" FAIL!");
    displayError(ERRORONTIMESET);
  }
  Serial << endl;
  //switch off the relay
  digitalWrite(RELEOFFPIN, HIGH);
  delay(RELEOFFPIN);
  digitalWrite(RELEOFFPIN, LOW);

  
  /* show the temperature time table on the serial line */

  int i;
  int j;
  for (i = 0; i < 7; i++) {
    for (j = 0; j < 24; j++) {
      setuptemp = (word)(pgm_read_dword(&(weektemp[i][j]))) / (word)10;
      Serial << setuptemp << F(" ");
    }
    Serial << endl;
  }
  time_t utc_t;
  utc_t = now();
  t = myTZ.toLocal(utc_t, &tcr);
  tLast3 = t;
  tLast2 = 0;
  tLast4 = t;
  tLast5 = t;
  tLast = t;
}

void loop(void)
{

  //check for input to set the RTC, minimum length is 12, i.e. yy,m,d,h,m,s
  if (Serial.available() >= 12){
    parseserial();
  }

  time_t utc_t;
  utc_t = now();
  t = myTZ.toLocal(utc_t, &tcr);
    //get the setup temp every hour and after an hour is has manually changed
    if ((t-tLast >= 3600) && (t-LastChanged >= 3600)) { 
      setuptemp = (word)(pgm_read_dword(&(weektemp[weekday(t) - 1][hour(t)]))) / (word)10;
      setuptemp = constrain(setuptemp, MINTEMP, MAXTEMP);
      Serial << F(" Temp set ") << setuptemp;
      tLast=t;
    }

    if(t != tLast3){ // show display every second
        showdisplay();
        tLast3 = t;
    }

    if ((t - tLast2) >= 60){ //  check the temperature every minute
      Serial << endl;
      printDateTime(t); //print date and time on the serial
      //if (second(t) == 0) {
      RTCTemp = RTC.temperature() / 4.;
      //float f = c * 9. / 5. + 32.;
      Serial << F("  ") << RTCTemp << F(" C  ") ;//<< f << F(" F");
      int chk = DHT11.read11(DHT11PIN);
      switch (chk)
      {
        case DHTLIB_OK:
          //Serial.println("OK");
          break;
        case DHTLIB_ERROR_CHECKSUM:
          Serial << endl << F("Checksum error");
          break;
        case DHTLIB_ERROR_TIMEOUT:
          Serial << endl << F("Time out error");
          break;
        default:
          Serial << endl << F("Unknown error");
          break;
      }
      DHT11Temp = DHT11.temperature + TEMP_OFFSET;
      DHT11Hum = DHT11.humidity + HUM_OFFSET;
      Serial << F("DHT11: Humidity (%): ");
      Serial << DHT11Hum;
      Serial << F(" Temperature (C): ");
      Serial << DHT11Temp;
      Serial << F(" Last Changed: ");
      printTime(LastChanged);
      Serial << F(" Lasted seconds: ") << (t - LastChanged);    
      Serial << F(" Day: ") << weekday(t) << F(" Hour: ") << hour(t) << F(" Set up Temperature ") << setuptemp;
      if ((setuptemp > DHT11Temp) && !running) {
        //switch on heating system
        digitalWrite(RELEONPIN, HIGH);
        delay(10);
        digitalWrite(RELEONPIN, LOW);
        running = true;
      }
      else {
        if ((setuptemp <= DHT11Temp) && running) {
          //switch off heating system
          digitalWrite(RELEOFFPIN, HIGH);
          delay(10);
          digitalWrite(RELEOFFPIN, LOW);
          running = false;
        }
      }
      tLast2 = t;
    }
    if ((t-tLast4)>=60){
      displaytype=1;
    }
    if((displaytype ==2) && ((t-tLast4)>=5)){
      displaytype=3;
    }
}

void displayError(int E)
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(3, 10);
  display.println(F("ERROR "));
  display.println(E);
  display.display();
  delay(ERRORDISPLAYDELAY);
}





//display an integer in "00" format (with leading zero),
//followed by a delimiter character to OLED.
//Input value assumed to be between 0 and 99.
void displayI00(int val, char delim)
{
  if (val < 10) display.print(F("0"));
  display.print(val);
  if (delim > 0) display.print(delim);
  return;
}


//print date and time to Serial
void printDateTime(time_t t)
{
  printDate(t);
  Serial << F(" ");
  printTime(t);
}


//print time to Serial
void printTime(time_t t)
{
  printI00(hour(t), ':');
  printI00(minute(t), ':');
  printI00(second(t), ' ');
}

//print date to Serial
void printDate(time_t t)
{
  printI00(day(t), 0);
  Serial << F(" ") << monthShortStr(month(t)) << F(" ") << _DEC(year(t));
}

//Print an integer in "00" format (with leading zero),
//followed by a delimiter character to Serial.
//Input value assumed to be between 0 and 99.
void printI00(int val, char delim)
{
  if (val < 10) Serial << F("0");
  Serial << _DEC(val);
  if (delim > 0) Serial << delim;
  return;
}


void inctemp()
{
  //Serial << endl << F("inctemp") << endl ;
  if (abs(millis() - bounceTime1) > BOUNCE_DURATION)
  {
    switch (displaytype) {
    case 1:
      displaytype = 3;
      showdisplay3();
    break;
    case 2:
      setuptemp = constrain(setuptemp + 1, MINTEMP, MAXTEMP);
      LastChanged = t;
      showdisplay2(); 
    break;
    case 3:
     displaytype = 2;
     showdisplay2();
    break;
    }
    tLast4=t;
    bounceTime1 = millis();
  }
}

void dectemp()
{
  //Serial << endl << F("dectemp") << endl ;
  if (abs(millis() - bounceTime2) > BOUNCE_DURATION)
  {
    switch (displaytype) {
    case 1:
      displaytype = 3;
      showdisplay3();
    break;
    case 2:
      setuptemp = constrain(setuptemp - 1, MINTEMP, MAXTEMP);
      LastChanged = t;
      showdisplay2(); 
    break;
    case 3:
     displaytype = 2;
     showdisplay2();
    break;
    }
    tLast4=t;
    bounceTime2 = millis();
  }
}

void showdisplay1(){
    display.clearDisplay();
    display.display();  
}


void showdisplay3()
{ //static boolean f;
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(15, 0);
  if (fx)
  {
    displayI00(hour(t), ':');
    fx = false;
  }
  else
  {
    displayI00(hour(t), ' ');
    fx = true;
  }
  if (running)
  {
    displayI00(minute(t), '.');
  }
  else
  {
    displayI00(minute(t), ' ');
  }
  display.setCursor(0, 30);
  display.print(int(DHT11Temp));
  display.print(F("C "));
  display.print(int(DHT11Hum));
  display.print(F("%"));
  display.display();
}

void showdisplay2()
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(3, 20);
  display.print(setuptemp);
  display.print(F(" C"));
  display.display();
}

void showdisplay()
{

    switch (displaytype) {
    case 1:
      showdisplay1();
    break;
    case 2:
      showdisplay2();
    break;
    case 3:
      showdisplay3();
    break;
  }
}
