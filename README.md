# UsbMscFat Library

# NOTE: This Library is OUTDATED !!!
# Please Use the examples in the latest USBHost_t36 stable library for the Teensy. Teensyduino 1.57.

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

- copyFilesUSB.ino	    Copy a file between USB drives and both SDIO and External SD cards.
                        The file named '32MEGfile.dat.zip' is supplied in the 'extras' folder.
                        unzip and copy it to one of the storage devices. You can then use that
                        to copy between the different storage devices.

- copyFileUSB.ino       Demonstrates copying, renaming and deleting a text file.

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
                        
- volumeName.ino        This sketch is an example of aquiring volume names from multiple partitions of multiple Fat types.

Error checking is still not completely functional yet. Mass storage sense key codes and additional sense codes are proccessed.
They are displayed as definitions of the error codes not the codes them selves. I have one PNY USB thumb drive that magically
decided to write protect itself and gave me this error when I tryed to do a direct sector write:

SCSI Transfer Failed: Code: 1 --> Type: DATA_PROTECT Cause: WRITE PROTECTED.

It turns out that this particular thumb drive had a factory firmware defect that needed an update. USB drives that I have don't seem to cause many failures using MSC so far so it was nice to be able to actually have it fail to test the sense codes.

