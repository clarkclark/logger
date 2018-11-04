# logger
Arduino project to fill a need for monitoring Relative Humidity (RH) and temp in my new house.
Based on an Arduino Nano clone and DFRobots LCD & button shield.
Uses a AM2302 (or AM2320) temp & RH module; a DS3231-based RTC module; an SD card module; and a photoresistor to monitor current light level.
Combined the RTC & SD card module into a generic data logging shield.
This device communicates using digital, serial, SPI, I2C.  (So far everything is wired, but there are still pins and addresses available for wireless.)
