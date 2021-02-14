/*
  MSC USB Drive test 
   
 This example shows how use the utility libraries on which the'
 SD library is based in order to get info about your USB Drive.
 Very useful for testing a card when you're not sure whether its working or not.
 	
 created  28 Mar 2011
 by Limor Fried 
 modified 9 Apr 2012
 by Tom Igoe
 modified 17 Nov 2020
 by Warren Watson
 */
 // include the SD library:
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
// but after it is initialized ther should be no delay.
msController msDrive1(myusb);
msController msDrive2(myusb);

const uint8_t SD_CS_PIN = 10;

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(60)
#define SPI_CLOCK1 SD_SCK_MHZ(46)

#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_CONFIG1 SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK1)

// set up variables using the SD utility library functions:
UsbFs msc1;
UsbFs msc2;
SdFs sdio;
SdFs spi;

FsFile file1;
FsFile file2;

const char *file2Copy = "32MEGfile.dat";  // File to copy

//------------------------------------------------------------------------------
// Check for a connected USB drive and mount if not mounted.
bool driveAvailable(msController *pDrive,UsbFs *mscVol) {
Serial.printf("mscVol = %lu\n",mscVol);
	if(pDrive->checkConnectedInitialized()) {
		return false;
	}
	if(!mscVol->fatType()) {
Serial.printf("mscVol = %lu\n",mscVol);
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------
// Check for a inserted SD card and mount if not mounted.
bool sdCardAvailable(SdFs *sdCard) {
	if(sdCard == &sdio) {
		if(!sdCard->begin(SD_CONFIG)) {
			Serial.printf("\nSDIO card is not available or not mounted\n");
			return false;
		}
	}
	if(sdCard == &spi) {
		if(!sdCard->begin(SD_CONFIG1)) {
			Serial.printf("\nExternal SD card is not available or not mounted\n");
			return false;
		}
	}
	return true;
}

// Copy a file from one drive to another.
// Set 'stats' the forth parameter to true to display a progress bar,
// copy speed and copy time. 
uint8_t fileCopy(bool stats) {
    int br = 0, bw = 0;          // File read/write count
	uint32_t bufferSize = 65536; // Buffer size can be adjusted as needed.
	uint8_t buffer[bufferSize];  // File copy buffer
	uint32_t cntr = 0;
	uint32_t start = 0, finish = 0;
	uint32_t bytesRW = 0;
    /* Copy source to destination */
	start = micros();
    for (;;) {
		if(stats) { // If true, display progress bar.
			cntr++;
			if(!(cntr%10)) Serial.printf("*");
			if(!(cntr%640)) Serial.printf("\n");
		}
        br = file1.read(buffer, sizeof(buffer));  /* Read a chunk of source file */
        if (br <= 0) {
			break; /* error or eof */
		}
        bw = file2.write(buffer, br);            /* Write it to the destination file */
        if (bw < br) {
			break; /* error or disk full */
		}
		bytesRW += (uint32_t)bw;
    }
	file2.sync(); // Flush write buffer.
    /* Close open files */
    file1.close();
    file2.close();
	finish = (micros() - start);
    float MegaBytes = (bytesRW*1.0f)/(1.0f*finish);
	if(stats) // If true, display time stats.
		Serial.printf("\nCopied %u bytes in %f seconds. Speed: %f MB/s\n",bytesRW,(1.0*finish)/1000000.0,MegaBytes);
	return 0; // Return any errors or success.
}

void listDirectories(void) {
	Serial.printf("-------------------------------------------------\n");
	Serial.printf("USB drive 1 directory listing:\n");
	msc1.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("USB drive 2 directory listing:\n");
	msc2.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("SDIO card directory listing:\n");
	sdio.ls("/", LS_R | LS_DATE | LS_SIZE);
	Serial.printf("External SD card directory listing:\n");
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

	Serial.printf("MULTI USB DRIVE FILE COPY TEST\n\n");
 
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
//    if(sdio.sdErrorCode() == SD_CARD_ERROR_ACMD41)
//      Serial.println("No card, Is SDIO card installed?");
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
	Serial.printf("\nPress a key to continue...\n");
	while(!Serial.available());
//	c = Serial.read();
}

void loop(void) {
	uint8_t c = 0;
	uint8_t copyResult = 0;

	Serial.printf("\n------------------------------------------------------------------\n");
	Serial.printf("Select:\n");
	Serial.printf("    1) to copy '32MEGfile.dat' from USB drive 1 to USB drive 2.\n");
	Serial.printf("    2) to copy '32MEGfile.dat' from USB drive 2 to USB drive 1.\n");
	Serial.printf("    3) to copy '32MEGfile.dat' from USB drive 1 to SDIO card.\n");
	Serial.printf("    4) to copy '32MEGfile.dat' from USB drive 2 to SDIO card.\n");
	Serial.printf("    5) to copy '32MEGfile.dat' from USB drive 1 to External SD card.\n");
	Serial.printf("    6) to copy '32MEGfile.dat' from USB drive 2 to External SD card.\n");
	Serial.printf("    7) to copy '32MEGfile.dat' from SDIO card to External SD card.\n");
	Serial.printf("    8) to copy '32MEGfile.dat' from External SD card to SDIO card.\n");
	Serial.printf("    9) to copy '32MEGfile.dat' from SDIO card to USB drive 1.\n");
	Serial.printf("    a) to copy '32MEGfile.dat' from SDIO card to USB drive 2.\n");
	Serial.printf("    b) to copy '32MEGfile.dat' from External SD card to USB drive 1.\n");
	Serial.printf("    c) to copy '32MEGfile.dat' from External SD card to USB drive 2.\n");
	Serial.printf("    d) List Directories\n");
	Serial.printf("------------------------------------------------------------------\n");

	while(!Serial.available());
	c = Serial.read();
	while(Serial.available()) Serial.read(); // Get rid of CR and/or LF if there.
	switch(c) {
		case '1':
			Serial.printf("Checking for USB Drive 1...\n");
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected or not mountable\n");
				break;
			}
			Serial.printf("Checking for USB Drive 2...\n");
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected or not mountable\n");
				break;
			}
			Serial.printf("\n1) Copying from USB drive 1 to USB drive 2\n");
			/* Open source file */
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '2':
			if(!driveAvailable(&msDrive2, &msc2)) {
				Serial.printf("USB Drive 2 is not connected\n");
				break;
			}
			if(!driveAvailable(&msDrive1, &msc1)) {
				Serial.printf("USB Drive 1 is not connected\n");
				break;
			}
			Serial.printf("\n2) Copying from USB drive 2 to USB drive 1\n");
			/* Open source file */
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '3':
			if(!driveAvailable(&msDrive1, &msc1) || !sdCardAvailable(&sdio)) break; // Is the drive available and mounted?
			Serial.printf("\n3) Copying from USB drive 1 to SDIO card\n");
			/* Open source file */
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&sdio,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '4':
			if(!driveAvailable(&msDrive2, &msc2) || !sdCardAvailable(&sdio)) break; // Is the drive available and mounted?
			Serial.printf("\n4) Copying from USB drive 2 to SDIO card\n");
			/* Open source file */
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '5':
			if(!driveAvailable(&msDrive1, &msc1) || !sdCardAvailable(&spi)) break; // Is the drive available and mounted?
			Serial.printf("\n5) Copying from USB drive 1 to External SD card\n");
			/* Open source file */
			if(!file1.open(&msc1,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '6':
			if(!driveAvailable(&msDrive2, &msc2) || !sdCardAvailable(&spi)) break; // Is the drive available and mounted?
			Serial.printf("\n6) Copying from USB drive 2 to External SD card\n");
			/* Open source file */
			if(!file1.open(&msc2,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '7':
			if(!sdCardAvailable(&sdio) || !sdCardAvailable(&spi)) break; // Is the drive available and mounted?
			Serial.printf("\n7) Copying from SDIO card to External SD card\n");
			/* Open source file */
			if(!file1.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&spi,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '8':
			if(!sdCardAvailable(&spi) || !sdCardAvailable(&sdio)) break; // Is the drive available and mounted?
			Serial.printf("\n8) Copying from External SD card to SDIO card\n");
			/* Open source file */
			if(!file1.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&sdio,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case '9':
			if(!sdCardAvailable(&sdio) || !driveAvailable(&msDrive1, &msc1)) break; // Is the drive available and mounted?
			Serial.printf("\n9) Copying from SDIO card to USB drive 1 \n");
			/* Open source file */
			if(!file1.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'a':
			if(!sdCardAvailable(&sdio) || !driveAvailable(&msDrive2, &msc2)) break; // Is the drive available and mounted?
			Serial.printf("\na) Copying from SDIO card to USB drive 2\n");
			/* Open source file */
			if(!file1.open(&sdio,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'b':
			if(!driveAvailable(&msDrive1, &msc1) || !sdCardAvailable(&spi)) break; // Is the drive available and mounted?
			Serial.printf("\nb) Copying from External SD card to USB drive 1 \n");
			/* Open source file */
			if(!file1.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc1,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
			if(copyResult != 0) {
				Serial.printf("File Copy Failed with code: %d\n",copyResult);
			}
			break;
		case 'c':
			if(!sdCardAvailable(&spi) || !driveAvailable(&msDrive2, &msc2)) break; // Is the drive available and mounted?
			Serial.printf("\nc) Copying from External SD card to USB drive 2\n");
			/* Open source file */
			if(!file1.open(&spi,file2Copy, O_RDONLY)) {
				Serial.printf("\nERROR: could not open source file: %s\n",file2Copy);
				break;
			}
			/* Create destination file on the drive 0 */
			if(!file2.open(&msc2,file2Copy, O_WRONLY | O_CREAT | O_TRUNC)) {
				Serial.printf("\nERROR: could not open destination file: %s\n",file2Copy);
				break;
			}
			copyResult = fileCopy(true);
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
	Serial.printf("\nPress a key to continue...\n");
	while(!Serial.available());
	c = Serial.read();
}
