# UsbMscFat Library

This Teensy 3.6/4.0/4.1 library allows use of FS and MSC with SdFat.

You will need Arduino 1.8.13 and TeensyDuino 1.54 Beta 6.

No other changes to FS, MSC or SdFat are needed. SdFat and MSC are now included in TeensyDuino 1.54B6.
It allows the use of USB mass storage devices along with exsisting SD cards and littleFS.

This library should be considered as proof of concept as PJRC is developing a standard interface for storage devices and may not be compatible with future releases of TeensyDuino.

Installation:
  Install this library in Arduino/libraries folder.
  
Examples:
- benchUSB.ino 
