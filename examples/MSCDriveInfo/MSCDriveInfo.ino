/*
  mscFS Drive Info test 
*/   
// include the SD-MSC library:
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
msController msDrive1(myusb);
msController msDrive2(myusb);

// set up variables using the mscFS utility library functions:
UsbFs myDrive1;
UsbFs myDrive2;

uint32_t volumesize;

// Show USB drive information for the selected USB drive.
int showUSBDriveInfo(msController  *drive) {
	// Print USB drive information.
	Serial.printf(F("   connected %d\n"),drive->msDriveInfo.connected);
	Serial.printf(F("   initialized %d\n"),drive->msDriveInfo.initialized);
	Serial.printf(F("   USB Vendor ID: %4.4x\n"),drive->msDriveInfo.idVendor);
	Serial.printf(F("  USB Product ID: %4.4x\n"),drive->msDriveInfo.idProduct);
	Serial.printf(F("      HUB Number: %d\n"),drive->msDriveInfo.hubNumber);
	Serial.printf(F("        HUB Port: %d\n"),drive->msDriveInfo.hubPort);
	Serial.printf(F("  Device Address: %d\n"),drive->msDriveInfo.deviceAddress);
	Serial.printf(F("Removable Device: "));
	if(drive->msDriveInfo.inquiry.Removable == 1)
		Serial.printf(F("YES\n"));
	else
		Serial.printf(F("NO\n"));
	Serial.printf(F("        VendorID: %8.8s\n"),drive->msDriveInfo.inquiry.VendorID);
	Serial.printf(F("       ProductID: %16.16s\n"),drive->msDriveInfo.inquiry.ProductID);
	Serial.printf(F("      RevisionID: %4.4s\n"),drive->msDriveInfo.inquiry.RevisionID);
	Serial.printf(F("         Version: %d\n"),drive->msDriveInfo.inquiry.Version);
	Serial.printf(F("    Sector Count: %ld\n"),drive->msDriveInfo.capacity.Blocks);
	Serial.printf(F("     Sector size: %ld\n"),drive->msDriveInfo.capacity.BlockSize);
	Serial.printf(F("   Disk Capacity: %.f Bytes\n\n"),(double_t)drive->msDriveInfo.capacity.Blocks *
										(double_t)drive->msDriveInfo.capacity.BlockSize);
	return 0;
}

void setup()
{
  
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect.
  }

	myusb.begin();
  Serial.println("\nInitializing USB MSC drive 1...");
  if (!myDrive1.begin(&msDrive1)) {
    Serial.print("initialization failed with code: ");
	Serial.println(myDrive1.mscErrorCode());
    return;
  }
  Serial.printf("myDrive1 Info:\n");
  showUSBDriveInfo(&msDrive1);
  myDrive1.ls(LS_R | LS_DATE | LS_SIZE);
  // print the type and size of the first FAT-type volume
  Serial.print("\nVolume type is FAT");
  Serial.println(myDrive1.fatType(), DEC);
//  Serial.println();
  Serial.print("Cluster Size (bytes): ");
  Serial.println(myDrive1.vol()->bytesPerCluster());
  volumesize = myDrive1.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= myDrive1.clusterCount();       // we'll have a lot of clusters
  if (volumesize < 8388608ul) {
    Serial.print("Volume size (bytes): ");
    Serial.println(volumesize * 512);        // USB drive blocks default to 512 bytes
  }
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 2;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);

  Serial.println("\nInitializing USB MSC drive 2...");
  if (!myDrive2.begin(&msDrive2)) {
    Serial.print("initialization failed with code: ");
	Serial.println(myDrive2.mscErrorCode());
    return;
  }
  Serial.printf("myDrive2 Info:\n");
  showUSBDriveInfo(&msDrive2);
  myDrive2.ls(LS_R | LS_DATE | LS_SIZE);

  // print the type and size of the first FAT-type volume
  volumesize = 0;
  Serial.print("\nVolume type is FAT");
  Serial.println(myDrive2.fatType(), DEC);
//  Serial.println();
  Serial.print("Cluster Size (bytes): ");
  Serial.println(myDrive2.vol()->bytesPerCluster());
  volumesize = myDrive2.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= myDrive2.clusterCount();       // we'll have a lot of clusters
  if (volumesize < 8388608ul) {
    Serial.print("Volume size (bytes): ");
    Serial.println(volumesize * 512);        // USB drive blocks default to 512 bytes
  }
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 2;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  
}


void loop(void) {
  
}
