/*
 * This program attempts to initialize an SD-MSC drive and analyze its structure.
 */
#include "mscFS.h"
#include "sdios.h"

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

// set up variables using the SD utility library functions:
UsbFs msc1;
static ArduinoOutStream cout(Serial);
//------------------------------------------------------------------------------
void errorPrint() {
  if (msc1.mscErrorCode()) {
    cout << F("msc1 errorCode: ") << hex << showbase;
    msc1.printMscError(&Serial);
    cout << F(" = ") << int(msc1.mscErrorCode()) << endl;
    cout << F("msc1 errorData = ") << int(msc1.mscErrorData()) << endl;
  }
}
//------------------------------------------------------------------------------
bool mbrDmp() {
  MbrSector_t mbr;
  bool valid = true;
  if (!msc1.usbDrive()->readSector(0, (uint8_t*)&mbr)) {
    cout << F("\nread MBR failed.\n");
    errorPrint();
    return false;
  }
  cout << F("\nmsc1 Partition Table\n");
  cout << F("part,boot,bgnCHS[3],type,endCHS[3],start,length\n");
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
//    if ((pt->boot != 0 && pt->boot != 0X80) ||
//        getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
//      valid = false;
//    }
    cout << int(ip) << ',' << uppercase << showbase << hex;
    cout << int(pt->boot) << ',';
    for (int i = 0; i < 3; i++ ) {
      cout << int(pt->beginCHS[i]) << ',';
    }
    cout << int(pt->type) << ',';
    for (int i = 0; i < 3; i++ ) {
      cout << int(pt->endCHS[i]) << ',';
    }
    cout << dec << getLe32(pt->relativeSectors) << ',';
    cout << getLe32(pt->totalSectors) << endl;
  }
  return true;
}
//------------------------------------------------------------------------------
void dmpVol() {
  cout << F("\nScanning FAT, please wait.\n");
  uint32_t freeClusterCount = msc1.freeClusterCount();
  if (msc1.fatType() <= 32) {
    cout << F("\nVolume is FAT") << int(msc1.fatType()) << endl;
  } else {
    cout << F("\nVolume is exFAT\n");
  }
  cout << F("sectorsPerCluster: ") << msc1.sectorsPerCluster() << endl;
  cout << F("clusterCount:      ") << msc1.clusterCount() << endl;
  cout << F("freeClusterCount:  ") << freeClusterCount << endl;
  cout << F("fatStartSector:    ") << msc1.fatStartSector() << endl;
  cout << F("dataStartSector:   ") << msc1.dataStartSector() << endl;
}
//------------------------------------------------------------------------------
void printCardType() {

  if(msc1.usbDrive()->usbType() == SD_CARD_TYPE_USB)
    cout << F("\nDrive type: USB Drive.");
 }

//-----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  // Wait for USB Serial
  while (!Serial) {
    SysCall::yield();
  }
  myusb.begin();
  cout << F("MSC Fat version: ") << MSC_FAT_VERSION << endl;

}
//------------------------------------------------------------------------------
void loop() {
  // Read any existing Serial data.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);

  // F stores strings in flash to save RAM
  cout << F("\ntype any character to start\n");
  while (!Serial.available()) {
    SysCall::yield();
  }
  uint32_t t = millis();
  if (!msc1.usbDriveBegin(&msDrive1)) {
    Serial.print("initialization failed with code: ");
	Serial.println(msc1.mscErrorCode());
    return;
  }
  t = millis() - t;
  cout << F("init time: ") << t << " ms" << endl;
  printCardType();
  if (!mbrDmp()) {
    return;
  }
  if (!msc1.volumeBegin()) {
    cout << F("\nvolumeBegin failed. Is the card formatted?\n");
    errorPrint();
    return;
  }
  dmpVol();
}
