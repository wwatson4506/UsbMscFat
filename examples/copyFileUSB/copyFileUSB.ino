/*
  MSC Drive read/write copy file
 
 This example shows how to read and write data to and from an SD card file 	
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11, pin 7 on Teensy with audio board
 ** MISO - pin 12
 ** CLK - pin 13, pin 14 on Teensy with audio board
 ** CS - pin 4, pin 10 on Teensy with audio board
 
 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 
 This example code is in the public domain.
 	 
 */
 
#include <mscFS.h>

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
msDevice msDrive(myusb);
msDevice msDrive1(myusb);

File myFile;
File myFile1;

// Size of read/write.
const size_t BUF_SIZE = 65536;

// File size in MB where MB = 1,024,000 bytes.
const uint32_t FILE_SIZE_MB = 32;

// File size in bytes.
const uint32_t FILE_SIZE = 1024000UL*FILE_SIZE_MB;
// Insure 4-byte alignment.
//uint32_t buf32[(BUF_SIZE + 3)/4];
//uint8_t* buf = (uint8_t*)buf32;
FASTRUN uint8_t* buf[BUF_SIZE];
uint32_t t;
uint32_t flSize = 0;
float MBs = 1.0f;

void setup()
{
 //UNCOMMENT THESE TWO LINES FOR TEENSY AUDIO BOARD:
 //SPI.setMOSI(7);  // Audio shield has MOSI on pin 7
 //SPI.setSCK(14);  // Audio shield has SCK on pin 14
  
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect.
  }

  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

  Serial.print("\nInitializing USB MSC drive...");
  if (!MSC.begin(&msDrive)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  File root = MSC.open("/");
  printDirectory(root, 0);

  // fill buf with known data
  if (BUF_SIZE > 1) {
    for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
      buf[i] = (uint8_t *)('A' + (i % 26));
    }
    buf[BUF_SIZE-2] = (uint8_t *)'\r';
  }
  buf[BUF_SIZE-1] = (uint8_t *)'\n';
  
  uint32_t n = FILE_SIZE/BUF_SIZE;

  if(MSC.exists("test.txt"))
	MSC.remove("test.txt");

  // open the file. 
  myFile = MSC.open("test.txt", FILE_WRITE_BEGIN);
  
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to test.txt...");
  t = millis();
  for (uint32_t i = 0; i < n; i++) {
    if (myFile.write(buf, BUF_SIZE) != BUF_SIZE) {
      Serial.printf("Write Failed: Stopping Here...");
      while(1);
    }
  }
  t = millis() - t;
  flSize = myFile.size();
  MBs = flSize / t;
  Serial.printf("Wrote %lu bytes %f seconds. Speed : %f MB/s\n",flSize, (1.0 * t)/1000.0f, MBs / 1000.0f);
  // close the file:
  myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("Error opening test.txt: Write Failed: Stoppiing Here...");
    while(1);
  }
  // re-open the file for reading:
  myFile = MSC.open("test.txt");
  if (myFile) {
    Serial.println("test.txt:");
	if(MSC.exists("copy.txt"))
		MSC.remove("copy.txt");
    // open the second file for writing. 
    myFile1 = MSC.open("copy.txt", FILE_WRITE_BEGIN);
    // if the file opened okay, write to it:
    if (myFile1) {
      Serial.printf("Copying test.txt to copy.txt...");
	  t = millis();
      while(myFile.read(buf, BUF_SIZE) == BUF_SIZE) {
        if (myFile1.write(buf, BUF_SIZE) != BUF_SIZE) {
          Serial.printf("Write Failed: Stoppiing Here...");
          while(1);
        }
      }
    }
    t = millis() - t;
    flSize = myFile.size();
    MBs = flSize / t;
    Serial.printf("Copied %lu bytes %f seconds. Speed : %f MB/s\n",flSize, (1.0 * t)/1000.0f, MBs/1000.0f);
    // close the files:
    myFile.close();
    myFile1.close();
  } else {
  	// if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  // re-open the second file for reading:
  myFile1 = MSC.open("copy.txt");
  if (myFile1) {
    Serial.println("copy.txt:");
    // open the file for a read. 
    myFile1 = MSC.open("copy.txt");
    // if the file opened okay, write to it:
    if (myFile1) {
      Serial.printf("Reading File: copy.txt...");
	  t = millis();
      while(myFile1.read(buf, BUF_SIZE) == BUF_SIZE);
    }
    t = millis() - t;
    flSize = myFile1.size();
    MBs = flSize / t;
    Serial.printf("Read %lu bytes %f seconds. Speed : %f MB/s\n",flSize, (1.0 * t)/1000.0f, MBs/1000.0f);
    // close the files:
    myFile1.close();
  } else {
  	// if the file didn't open, print an error:
    Serial.println("Error opening copy.txt");
  }
  Serial.printf("Done..\n");
}

void loop()
{
	// nothing happens after setup
}

void printDirectory(File dir, int numSpaces) {
   while(true) {
     File entry = dir.openNextFile();
     if (! entry) {
       //Serial.println("** no more files **");
       break;
     }
     printSpaces(numSpaces);
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numSpaces+2);
     } else {
       // files have sizes, directories do not
       printSpaces(48 - numSpaces - strlen(entry.name()));
       Serial.print("  ");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

void printSpaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(" ");
  }
}


