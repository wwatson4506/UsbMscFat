# UsbMscFat Library

This Teensy 3.6/4.0/4.1 library allows use of FS and MSC with SdFat.

You will need Arduino 1.8.13 and TeensyDuino 1.54 Beta 6.

No other changes to FS, MSC or SdFat are needed. SdFat and MSC are now included in TeensyDuino 1.54B6.
It allows the use of USB mass storage devices along with exsisting SD cards and littleFS.

This library should be considered as proof of concept as PJRC is developing a standard interface for storage devices and may not
be compatible with future releases of TeensyDuino.

Installation:

Install this library in Arduino/libraries folder.
  
Examples:
- benchUSB.ino		      Test USB drive read and write speeds. Modified from SdFat example for
                        use with USB drives.

- CardInfoUSB.ino	      Get USB drive information.

- copyFilesUSB.ino	    Copy a 32Meg file between USB drives and both SDIO and External SD cards.
                        The file named '32MEGfile.dat.zip' is supplied in the 'extras' folder.
                        unzip and copy it to one of the storage devices. You can then use that
                        to copy between the different storage devices.

- copyFileUSB.ino       Demonstrates copying, renameing and deleteing a text file.

- DataloggerUSB.ino     Original from SD modified for USB drives.

- DumpFile.ino          Original from SD modified for USB drives.

- ExFatFormatterUSB.ino Formats USB drives to SdFat ExFat.

- FilesUSB.ino          Original from SD modified for USB drives.

- listFiles.ino         Original from SD modified for USB drives.

- MSCDriveInfo.ino      Gives low level information about USB drives attached.

- ReadWriteUSB.ino      Original from SD modified for USB drives.

- SdInfoUSB.ino         Modified from SdFat example for use with USB drives.

- WaveFilePlayerUSB.ino This is a modfied version of WaveFilePlayer.ino from the Audio library that
                        works with USB Mass Storage devices.
                        
