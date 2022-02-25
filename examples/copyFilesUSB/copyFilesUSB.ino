/*
  Multi MSC USB Drive and SD card filecopy testing. 
   
 This example shows how use the mscFS and SD libraries to copy files.
 between USB, SDIO and External SD card devices. It also demonstrates
 hot plugging both USB drives and SD cards. There are two functions that
 do this. They both will try to re-mount the devices if they are not
 mounted.
 	
 Created 2-15-2021
 by Warren Watson
*/

#include "SPI.h"
#include "Arduino.h"
#include "mscFS.h"
#include "SD.h"

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// Setup MSC for the number of USB Drives you are using. (Two for this example)
// Mutiple  USB drives can be used. Hot plugging is supported. There is a slight
// delay after a USB MSC device is plugged in. This is waiting for initialization
// but after it is initialized there should be no delay.
msDevice msDrive1(myusb);
msDevice msDrive2(myusb);

const uint8_t SD_CS_PIN = 10;

// Try max SPI clock for an SDIO dard. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(60)
// Try max SPI clock for an SD card. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK1 SD_SCK_MHZ(46)
// Setup the SD card device configs.
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_CONFIG1 SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK1)

// Create USB and SD instances. Two USB drives and two SD cards.
UsbFs msc1;
UsbFs msc2;
SdFs sdio;
SdFs spi;

// Create SdFat source and destination file pointers.
PFsFile file1; // USB srcType
PFsFile file2; // USB destType
FsFile file3;  // SD srcType
FsFile file4;   // SD destType

// File to copy. This file is provided in the extras folder in ths library.
// Change this to any other file you wish to copy.
const char *file2Copy = "32MEGfile.dat";

//------------------------------------------------------------------------------
// Check for a connected USB drive and try to mount if not mounted.
bool driveAvailable(msDevice *pDrive,UsbFs *mscVol) {
	if(pDrive->checkConnectedInitialized()) {
		return false; // No USB Drive connected, give up!
	}
	if(!mscVol->fatType()) {  // USB drive present try mount it.
		if (!mscVol->begin(&msDrive1)) {
			mscVol->initErrorPrint(&Serial); // Could not mount it print reason.
			return false;
		}
	}
	return true;
}

//------------------------------------------------------------------------------
// Check for a inserted SD card and try to mount if not mounted.
bool sdCardAvailable(SdFs *sdCard) {
	if(sdCard == &sdio) {
		if(!sdCard->begin(SD_CONFIG)) {
			return false;
		}
	}
	if(sdCard == &spi) {
		if(!sdCard->begin(SD_CONFIG1)) {
			return false;
		}
	}
	return true;
}

// Copy a file from one drive to another.
// Set 'stats' to true to display a progress bar,
// copy speed and copy time. 
int fileCopy(bool srcType, bool destType, bool stats) {
    int br = 0, bw = 0;          // File read/write count
	uint32_t bufferSize = 32*1024; // Buffer size. Play with this:)
	uint8_t buffer[bufferSize];  // File copy buffer
	uint32_t cntr = 0;
	uint32_t start = 0, finish = 0;
	uint32_t bytesRW = 0;
	int copyError = 0;
	
    /* Copy source to destination */
	start = micros();
    for (;;) {
		if(stats) { // If true, display progress bar.
			cntr++;
			if(!(cntr % 10)) Serial.printf("*");
			if(!(cntr % 640)) Serial.printf("\n");
		}
        if(srcType)
			br = file1.read(buffer, sizeof(buffer));  // Read buffer size of source file (USB Type)
        else
			br = file3.read(buffer, sizeof(buffer));  // Read buffer size of source file (SD Type)
        
        if (br <= 0) {
			copyError = br;
			break; // Error or EOF
		}
        if(destType)
			bw = file2.write(buffer, br); // Write it to the destination file (USB Type)
        else
			bw = file4.write(buffer, br); // Write it to the destination file (SD Type)
        
        if (bw < br) {
			copyError = bw; // Error or disk is full
			break;
		}
		bytesRW += (uint32_t)bw;
    }
	if(destType)
		file2.sync(); // Flush write buffer.
    else
    	file4.sync(); // Flush write buffer.
    // Close open files
	if(srcType)
		file1.close(); // Source
	else
	    file3.close();
    if(destType)
		file2.close(); // Destination
	else
		file4.close(); // Destination
	finish = (micros() - start); // Get total copy time.
    float MegaBytes = (bytesRW*1.0f)/(1.0f*finish);
	if(stats) // If true, display time stats.
		Serial.printf("\nCopied %u bytes in %f seconds. Speed: %f MB/s\n",
		                 bytesRW,(1.0*finish)/1000000.0,MegaBytes);
	return copyError; // Return any errors or success.
}

void listDirectories(void) {
	Serial.printf("-------------------------------------------------\n");
	Serial.printf("\nUSB drive 1 directory listing:\n");
	msc1.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("\nUSB drive 2 directory listing:\n");
	msc2.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("\nSDIO card directory listing:\n");
	sdio.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("\nExternal SD card directory listing:\n");
	spi.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("-------------------------------------------------\n");
}

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  Serial.printf("MULTI USB DRIVE AND SD CARD FILE COPY TESTING\n\n");
 
  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

  // Initialize USB drive 1
  Serial.print("Initializing USB MSC drive 1...");
  if (!msc1.begin(&msDrive1)) {
	msc1.initErrorPrint(&Serial);
  } else {
     Serial.println("USB drive 1 is present.");
  }
  
  // Initialize USB drive 2
  Serial.print("\nInitializing USB MSC drive 2...");
  if (!msc2.begin(&msDrive2)) {
	msc2.initErrorPrint(&Serial);
  } else {
     Serial.println("USB drive 2 is present.");
  }

  // Initialize SDIO card
  Serial.print("\nInitializing SDIO card...");
  if (!sdio.begin(SD_CONFIG)) {
	sdio.initErrorPrint(&Serial);
  } else {
     Serial.println("SDIO card is present.");
  }

  // Initialize External SD card
  Serial.print("\nInitializing External SD card...");
  if (!spi.begin(SD_CONFIG1)) {
	spi.initErrorPrint(&Serial);
  } else {
     Serial.println("External SD card is present.");
  }
}

void loop(void) {
	uint8_t c = 0;
	int copyResult = 0;

	Serial.printf("\n------------------------------------------------------------------\n");
	Serial.printf("Select:\n");
	Serial.printf("   1)  to copy '%s' from USB drive 1 to USB drive 2.\n", file2Copy);
	Serial.printf("   2)  to copy '%s' from USB drive 2 to USB drive 1.\n", file2Copy);
	Serial.printf("   3)  to copy '%s' from USB drive 1 to SDIO card.\n", file2Copy);
	Serial.printf("   4)  to copy '%s' from USB drive 2 to SDIO card.\n", file2Copy);
	Serial.printf("   5)  to copy '%s' from USB drive 1 to External SD card.\n", file2Copy);
	Serial.printf("   6)  to copy '%s' from USB drive 2 to External SD card.\n", file2Copy);
	Serial.printf("   7)  to copy '%s' from SDIO card to External SD card.\n", file2Copy);
	Serial.printf("   8)  to copy '%s' from External SD card to SDIO card.\n", file2Copy);
	Serial.printf("   9)  to copy '%s' from SDIO card to USB drive 1.\n", file2Copy);
	Serial.printf("   a)  to copy '%s' from SDIO card to USB drive 2.\n", file2Copy);
	Serial.printf("   b)  to copy '%s' from External SD card to USB drive 1.\n", file2Copy);
	Serial.printf("   c)  to copy '%s' from External SD card to USB drive 2.\n", file2Copy);
	Serial.printf("   d)  List Directories\n");
	Serial.printf("------------------------------------------------------------------\n");

	while(!Serial.available());
	c = Serial.read();
	while(Serial.available()) Serial.read(); // Get rid of CR and/or LF if there.

	// This is a rather large and bloated switch() statement. And there are better ways to do this
	// but it served the quick copy paste modify senario:)
	switch(c) {
		case '1':
			if(!driveAvailable(&msDrive1, &msc1)) { // Check for USB drive 1 connected and mounted.
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive2, &msc2)) { // Check for USB drive 2 connected and mounted.
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			Serial.printf("\n1) Copying from USB drive 1 to USB drive 2\n");
			// Attempt to open source file
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			// Attempt to create destination file
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;

		case '2':
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			Serial.printf("\n2) Copying from USB drive 2 to USB drive 1\n");
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;

		case '3':
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n3) Copying from USB drive 1 to SDIO card\n");
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&sdio,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;

		case '4':
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n4) Copying from USB drive 2 to SDIO card\n");
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&sdio,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '5':
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n5) Copying from USB drive 1 to External SD card\n");
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '6':
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n6) Copying from USB drive 2 to External SD card\n");
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '7':
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n7) Copying from SDIO card to External SD card\n");
			if(!file3.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '8':
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			Serial.printf("\n8) Copying from External SD card to SDIO card\n");
			if(!file3.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file4.open(&sdio,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, false, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '9':
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			Serial.printf("\n9) Copying from SDIO card to USB drive 1 \n");
			if(!file3.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'a':
			if(!sdCardAvailable(&sdio)) {
				Serial.printf("SDIO card is not inserted or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&sdio) || !driveAvailable(&msDrive2, &msc2)) break; // Is the drive available and mounted?
			Serial.printf("\na) Copying from SDIO card to USB drive 2\n");
			if(!file3.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'b':
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			Serial.printf("\nb) Copying from External SD card to USB drive 1 \n");
			if(!file3.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'c':
			if(!sdCardAvailable(&spi)) {
				Serial.printf("External SD card is not inserted or not mountable\n");
				break;
			}
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			if(!sdCardAvailable(&spi) || !driveAvailable(&msDrive2, &msc2)) break; // Is the drive available and mounted?
			Serial.printf("\nc) Copying from External SD card to USB drive 2\n");
			if(!file3.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(false, true, true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'd':
			listDirectories();
			break;
		default:
			break;
	}
}
