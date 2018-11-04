/* 2018-11-04 cewood  https://github.com/clarkclark/logger     build as of 2018-10-21
*     Arduino project to fill a need for monitoring Relative Humidity (RH) and temp in my new house.
*     Started with an Arduino Uno and a Chinese clone DFRobots LCD/button module. 
*     Migrated to a Chinese Nano clone on a breakout board (appears as Arduino Duemilanove) with CH340 USB-serial chip.
*     Used AM2302 temp & RH module, learned to manipulate the output and displayed current RH & Temp on the LCD.
*        (later moved to AM2320 sensors for perhaps better accuracy)
*     Values changed over time, so I added a DS3231-based RTC module, connecting with I2C, to display current time.
*     I'm a BIG fan of ISO8601, but the RTC wants to talk mm/dd/yy like an American. Jumped through hoops to (correctly!) format the date/time.
*     Use Adafruit's RTC library, MUCH simpler, still requires manual padding for ISO8601.
*     To evaluate changes over time, I added an SD card module to LOG each observation, which communicates using SPI (mosi/miso).
*     Sending data to the serial port in real time, so I can see what's being recorded without stopping to read the SD card.
*     Since I STILL hadn't used all the I/O pins on the Nano, I added a photoresistor to monitor current light level (arbitrary value).
*        (photoresistor between +5 volts and Analog pin A2        10K ohm resistor between ground and Analog pin A2)
*     Best way to output all this stuff would be to buffer the output and then print each line: to the LCD, the serial monitor, and the SD card.
*        However, to print the degree symbol on the serial monitor, 
*            Serial.print("\xC2");      // requires two-byte unicode to write to serial monitor
*            Serial.print("\xB0"); 
*        to print it on my LCD display,
*            lcd.print((char)223);      //the degree symbol 223 works here!
*        and to print it to my data logging file on an SD card,
*            myFile.print((char)176);   // degree symbol 176 works correctly for SD write
*        So I settled for three separate output stanzas.
*     At the request of a statistician friend, I also output data to a second SD card file to produce standard machine-readable csv data.  No comments.
*     LCD backlight is on PWM pin 10 (hardwired on the DFRobot shield), so I fade it in and out to show when it's safe to remove the SD card
*     Turns out the 15-second cycle of dark, fade backlight to bright, fade backlight to dark, repeat is very restful as a bedroom nightlight.
*     I had a 15-second delay between observations, so I spent that time waiting for a button-press on the DFRobot shield:
*        I send these to the SD card     left: "event 1";  right "event 2";  up "new location";  down "back home"
*        since this logger is portable but I'm not storing GPS location, this lets me insert remarks in the file for later review.
*        Also, the select button: full back light for a quick read during the dark period
*     
*     I built a data-logging shield, includes SD card module and RTC module.  Hey! this is a universal data-logging device, not just RH & Temp!
*     
*     So this device communicates using digital, serial, SPI, I2C.  (So far everything is wired, but there are still pins and addresses available for wireless.)
*
*/

  // LIBRARY... including libraries and global variables
#include <RTClib.h>        		// RTClib (Adafruit) ver 1.2.0 installed with Library Manager
#include <LiquidCrystal.h>		// dfRobot LCD shield
#include <DHT.h>		        // AM2302 temp & RH module (works for AM2320 as well)
#include <Wire.h>		        // I2C to talk to AM2302 temp&RH module (and any other I2C devices)
#include <SPI.h>	        	// to talk to an SPI (mosi&miso) device
#include <SD.h>			        // to talk to the SD card USING SPI
#define DHTPIN 15		        // pin D15 is Analog pin A1 for temp&RH sensor; cleaner wiring than using a digital pin
#define DS3231_I2C_ADDRESS 0x68		// this is the I2C address for the RTC on the bus
#define DHTTYPE DHT22		  	// DHT 22  (AM2302), AM2321
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);	// select the DIGITAL pins used by the LCD PANEL (RS E D4 D5 D6 D7)
const int cs = 3;			// define Chip Select pin for SPI communication; the usual pin D10 (LCD backlight control) is not part of the 6-pin SPI
					// this pin stays +5V the whole time the sketch runs. It COULD be tied to Vcc and leave pin D3 available.
					// unless, of course, there's another SPDI device and they both get selected in turn.
int sensorPin = A2;    			// select the input pin for the photoresistor
int sensorValue = 0;  			// variable to store the value coming from the photoresistor
DHT dht(DHTPIN, DHTTYPE);		// mystery command for the temp/RH sensor. The previous 'define' statements resolve to: DHT dht(15, DHT22);
RTC_DS3231 rtc;
File myFile;				// for SD logging
File NicoFile;				// for SD logging

// define some values used by the LCD panel buttons
int lcd_key     = 0;
int adc_key_in  = 0;

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

int read_LCD_buttons(){               // I think this is a PROCESS to read the buttons
    adc_key_in = analogRead(0);       // read the value from the sensor 

    // my buttons when read are centered at these values: 0, 144, 329, 504, 741
    // we add approx 50 to those values and check to see if we are close
    // We make btnNONE the first option since it will be the most likely result

    if (adc_key_in > 1000) return btnNONE; 

   // For board version V1.0 (including Chinese clones)
     if (adc_key_in < 50)   return btnRIGHT;  
     if (adc_key_in < 195)  return btnUP; 
     if (adc_key_in < 380)  return btnDOWN; 
     if (adc_key_in < 555)  return btnLEFT; 
     if (adc_key_in < 790)  return btnSELECT;   

    return btnNONE;                // when all others fail, return this. Unneeded?
}


  // SETUP... setup code, to run once 
void setup()
{
  lcd.begin(16,2);			// define writable LCD space
  Wire.begin();				// talk to the RTC with I2C
  dht.begin();				// read the temp sensor
  pinMode(10, OUTPUT);			// PWM pin D10 controls the backlight on the LCD

  Serial.begin(9600);			// for troubleshooting, but we have plenty of time so leave it functioning
  Serial.println("data logging ****");

  myFile = SD.open("datalog.txt", FILE_WRITE);	// these steps happen for every SD card write
  NicoFile = SD.open("nico-log.txt", FILE_WRITE);	// these steps happen for every SD card write
  myFile.println("data logging ****");
  NicoFile.println("Hello Nico");
  myFile.close();
  NicoFile.close();

if (!SD.begin(cs))			// ! means 'not'
{
Serial.println("SD Card failed or not present");
}

/*
// looks like the clock set from PC time works just fine, but I'll leave this stanza remarked out anyway.
  if (! rtc.begin()) {				// from Adafruit example ds3231.ino for RTClib
    Serial.println("Couldn't find RTC");
    while (1);
  }
    if (rtc.lostPower()) {	// not sure what would trigger this, but it only works if we're attached to the PC
    Serial.println("RTC lost power, let's set the time!");
    // following line sets the RTC to the date & time this sketch WAS COMPILED (PC time)
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call (OUTSIDE this conditional):
    //  rtc.adjust(DateTime(2018, 10, 13, 21, 30, 00));
    }
*/
    // This line sets the RTC with an explicit date & time, as (yyyy, mm, dd, hh, mm, ss)
    // unremark and update next line to manually set the time
    // rtc.adjust(DateTime(2018, 10, 14, 19, 49, 00));
    // THEN REMARK IT AND UPLOAD AGAIN, or every restart will reset to this time!

}

  //LOOP; the program that runs over and over
void loop()
{
    // fade the LCD backlight in and out while operations are occurring.
    // don't remove  the SD card when the backlight is on!
    //
    analogWrite(10, 0);		// start with a dark screen backlight

    // while it's dark, read the value from the photoresistor
    sensorValue = analogRead(sensorPin);	// (values roughly between 100 and 1000)

    // fade the backlight ON
    for (int i=0; i <= 255; i++)
    {
    analogWrite(10, i);		// set the brightness of pin 10 (LCD backlight)
    delay(20);			// 255 steps of 20 mSec each is about 5 seconds
    }

   	// Read humidity
  float h = dht.readHumidity();
  	// Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  	// Read temperature as Celsius (the default) We don't need this
    //  float t = dht.readTemperature();

    DateTime now = rtc.now();

//	writing to serial **************************************************************************
  // here we hand-assemble an ISO 8601 date/time string
  Serial.print(now.year(), DEC);
  Serial.print("-");          // single quote would make this a minus operation
  if (now.month()<10)
  {
    Serial.print("0");
  }
  Serial.print(now.month(), DEC);
  Serial.print("-");
  if (now.day()<10)
  {
    Serial.print("0");
  }
  Serial.print(now.day(), DEC);
  Serial.print('T');
  if (now.hour()<10)
  {
    Serial.print("0");
  }
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  if (now.minute()<10)
  {
    Serial.print("0");
  }
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  if (now.second()<10)
  {
    Serial.print("0");
  }
  Serial.print(now.second(), DEC);
  Serial.print((char)9);	// the tab symbol
// now for the data
  Serial.print(f);		// Serial.print("XX.XX"); is the default
	//  Serial.print(f,1);		// Serial.print("XX.X"); limited to 1 decimal place
  Serial.print("\xC2");		// degree symbol requires two-byte unicode for Serial.print
  Serial.print("\xB0"); 
  Serial.print("F  RH ");
  Serial.print(h);		//Serial.print("XX.XX"); is the default
  Serial.print("%");
  Serial.print((char)9);    // the tab symbol
  Serial.print("light level is ");
  Serial.print(sensorValue);
  Serial.println();


// writing to SD card **************************************************************************

  myFile = SD.open("datalog.txt", FILE_WRITE);
  // here we hand-assemble an ISO 8601 date/time string
  myFile.print(now.year(), DEC);
  myFile.print("-");          // single quote would make this a minus operation
  if (now.month()<10)
  {
    myFile.print("0");
  }
  myFile.print(now.month(), DEC);
  myFile.print("-");
  if (now.day()<10)
  {
    myFile.print("0");
  }
  myFile.print(now.day(), DEC);
  myFile.print('T');
  if (now.hour()<10)
  {
    myFile.print("0");
  }
  myFile.print(now.hour(), DEC);
  myFile.print(':');
  if (now.minute()<10)
  {
    myFile.print("0");
  }
  myFile.print(now.minute(), DEC);
  myFile.print(':');
  if (now.second()<10)
  {
    myFile.print("0");
  }
  myFile.print(now.second(), DEC);
  myFile.print((char)9);		// should be a tab symbol
// now for the data
  myFile.print(f);			// myFile.print("XX.XX"); default
  myFile.print((char)176);		// degree symbol for SD write
  myFile.print("F  RH ");
  myFile.print(h);			//myFile.print("XX.XX");
  myFile.print("%");
  myFile.print((char)9);		// should be a tab symbol
  myFile.print("light level is ");
  myFile.print(sensorValue);
  myFile.println();

  myFile.close();

// writing csv file to SD card **************************************************************************
// target format for Nico's version of the file	"date, time, temp, RH, light"	"20181014, 202802, 69.62, 48.40, 786"
  NicoFile = SD.open("nico-log.txt", FILE_WRITE);

  // here we hand-assemble an ISO 8601 date/time string
  NicoFile.print(now.year(), DEC);
   if (now.month()<10)
   {
    NicoFile.print("0");
   }
  NicoFile.print(now.month(), DEC);
  if (now.day()<10)
  {
    NicoFile.print("0");
  }
  NicoFile.print(now.day(), DEC);
  NicoFile.print(", ");
  if (now.hour()<10)
  {
    NicoFile.print("0");
  }
  NicoFile.print(now.hour(), DEC);
  if (now.minute()<10)
  {
    NicoFile.print("0");
  }
  NicoFile.print(now.minute(), DEC);
  if (now.second()<10)
  {
    NicoFile.print("0");
  }
  NicoFile.print(now.second(), DEC);
  NicoFile.print(", ");
// now for the data
  NicoFile.print(f);			// NicoFile.print("XX.XX"); default
  NicoFile.print(", ");
  NicoFile.print(h);			//NicoFile.print("XX.XX");
  NicoFile.print(", ");
  NicoFile.print(sensorValue);
  NicoFile.println();

  NicoFile.close();

// writing to 16 x 02 LCD display **************************************************************************

  lcd.setCursor(0,0);      	// set the LCD cursor position, first column of upper row
  // here we hand-assemble an ISO 8601 date/time string
  lcd.print(now.year(), DEC);
  lcd.print("-");          // single quote would make this a minus operation
  if (now.month()<10)
  {
    lcd.print("0");
  }
  lcd.print(now.month(), DEC);
  lcd.print("-");
  if (now.day()<10)
  {
    lcd.print("0");
  }
  lcd.print(now.day(), DEC);
  lcd.print('T');
  if (now.hour()<10)
  {
    lcd.print("0");
  }
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  if (now.minute()<10)
  {
    lcd.print("0");
  }
  lcd.print(now.minute(), DEC);
					//    lcd.print(':');  no room on the LCD
					//    lcd.print(now.second(), DEC);
    
// LINE 2, first column
  lcd.setCursor(0,1);
  lcd.print(f,1);			// lcd.print("XX.X");
  lcd.print((char)223);			//the degree symbol for lcd.print
  lcd.print("F  RH ");
  lcd.print(h,1);			//lcd.print("XX.X");
  lcd.print("%");
// no screen real estate to display light level so skip it.

// all three or four outputs are complete **************************************************************************

    // now we fade out the backlight
    for (int i=255; i >= 0; i--)
    {
    analogWrite(10, i);			// set the brightness of pin 10 (LCD backlight)
    delay(20);				// 255 steps of 20 mSec each is about 5 seconds
    }

//  delay (5000);       // fade-in and fade-out are about 5 seconds each, so this should give us 4 samples per minute

    for (int i=0; i <= 255; i++)     // the alternative to a 5 second rest is to read the buttons for 5 seconds (while LCD backlight is OFF)
    {
     lcd_key = read_LCD_buttons();   // read the buttons
     switch (lcd_key){               // depending on which button was pushed, we perform an action

       case btnRIGHT:{
            Serial.println("event 2");
            myFile = SD.open("datalog.txt", FILE_WRITE);
            myFile.println("event 2");
            myFile.close();
            break;
       }
       case btnLEFT:{
            Serial.println("event 1");
            myFile = SD.open("datalog.txt", FILE_WRITE);
            myFile.println("event 1");
            myFile.close();
             break;
       }    
       case btnUP:{
            Serial.println("new location");
            myFile = SD.open("datalog.txt", FILE_WRITE);
            myFile.println("new location");
            myFile.close();
             break;
       }
       case btnDOWN:{
            Serial.println("return home");
            myFile = SD.open("datalog.txt", FILE_WRITE);
            myFile.println("return home");
            myFile.close();
             break;
       }
       case btnSELECT:{
            analogWrite(10, 255);    // set the brightness of pin 10 (LCD backlight) to FULL for 5 seconds
            delay(5000);
             break;
       }
     }
    delay(20);
   }
	// that spent five seconds (and maybe performed an action) Let's do it all again!
}
