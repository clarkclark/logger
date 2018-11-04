# logger
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
