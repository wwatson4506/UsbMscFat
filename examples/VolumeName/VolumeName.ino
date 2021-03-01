//  VolumeName.ino
// VER: P494, p498, p506, p507. p522
//  An example of how to retrieve Fat32 and ExFat volume names using SdFat.
//  Works with SD cards and USB mass storage drives.

#include "Arduino.h"
#include "mscFS.h"

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

#define SHOW_CLOCK_CARAT 1
IntervalTimer clocked100ms;
volatile int32_t cmsReport = -1;

// Setup MSC for the number of USB Drives you are using. (Two for this example)
// Mutiple  USB drives can be used. Hot plugging is supported. There is a slight
// delay after a USB MSC device is plugged in. This is waiting for initialization
// but after it is initialized ther should be no delay.
msController msDrive1(myusb);
msController msDrive2(myusb);

#define SD_DRIVE 1
#define MS_DRIVE 2

#define SD_CONFIG SdioConfig(FIFO_SDIO)

// set up variables using the mscFS utility library functions:
UsbFs msc1;
UsbFs msc2;

SdFs sd;


bool getPartitionVolumeLabel(PFsVolume &partVol, uint8_t *pszVolName, uint16_t cb) {
  uint8_t buf[512];
  if (!pszVolName || (cb < 12)) return false; // don't want to deal with it

  PFsFile root;
  if (!root.openRoot(&partVol)) return false;
  root.read(buf, 32);
  //print_hexbytes(buf, 32);

  switch (partVol.fatType())
  {
    case FAT_TYPE_FAT12:
    case FAT_TYPE_FAT16:
    case FAT_TYPE_FAT32:
      {
        DirFat_t *dir;
        dir = reinterpret_cast<DirFat_t*>(buf);
        if ((dir->attributes & 0x08) == 0) return false; // not a directory...
        size_t i;
        for (i = 0; i < 11; i++) {
          pszVolName[i]  = dir->name[i];
        }
        while ((i > 0) && (pszVolName[i - 1] == ' ')) i--; // trim off trailing blanks
        pszVolName[i] = 0;
      }
      break;
    case FAT_TYPE_EXFAT:
      {
        DirLabel_t *dir;
        dir = reinterpret_cast<DirLabel_t*>(buf);
        if (dir->type != EXFAT_TYPE_LABEL) return false; // not a label?
        size_t i;
        for (i = 0; i < dir->labelLength; i++) {
          pszVolName[i] = dir->unicode[2 * i];
        }
        pszVolName[i] = 0;
      }
      break;
  }
  return true;
}



bool mbrDmp(BlockDeviceInterface *blockDev) {
  MbrSector_t mbr;
  // bool valid = true;
  if (!blockDev->readSector(0, (uint8_t*)&mbr)) {
    Serial.print("\nread MBR failed.\n");
    //errorPrint();
    return false;
  }
  Serial.print("\nmsc # Partition Table\n");
  Serial.print("\tpart,boot,bgnCHS[3],type,endCHS[3],start,length\n");
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
    //    if ((pt->boot != 0 && pt->boot != 0X80) ||
    //        getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
    //      valid = false;
    //    }
    switch (pt->type) {
      case 4:
      case 6:
      case 0xe:
        Serial.print("FAT16:\t");
        break;
      case 11:
      case 12:
        Serial.print("FAT32:\t");
        break;
      case 7:
        Serial.print("exFAT:\t");
        break;
      default:
        Serial.print("pt_#");
        Serial.print(pt->type);
        Serial.print(":\t");
        break;
    }
    Serial.print( int(ip)); Serial.print( ',');
    Serial.print(int(pt->boot), HEX); Serial.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serial.print("0x"); Serial.print(int(pt->beginCHS[i]), HEX); Serial.print( ',');
    }
    Serial.print("0x"); Serial.print(int(pt->type), HEX); Serial.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serial.print("0x"); Serial.print(int(pt->endCHS[i]), HEX); Serial.print( ',');
    }
    Serial.print(getLe32(pt->relativeSectors), DEC); Serial.print(',');
    Serial.println(getLe32(pt->totalSectors));
  }
  return true;
}

void setup()
{
#if 0 // easy test to check HardFault Detection response
  int *pp = 0;
  *pp = 5;
#endif
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

#ifdef SHOW_CLOCK_CARAT
  clocked100ms.begin(clock_isr, 100000);
#endif

}

void clock_isr() {
  if (cmsReport >= 0 ) {
    if (cmsReport > 0 ) {
      if (cmsReport < 10 )
        Serial.print( "^");
      else if ( cmsReport < 50 && !(cmsReport % 10) )
        Serial.print( "~");
      else if ( !(cmsReport % 50) )
        Serial.print( ":(");
    }
    cmsReport++;
  }
}

void print_hexbytes(const void *ptr, int len)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%02X ", p[i]);
    }
    Serial.print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    Serial.println();
    p += 32;
    len -= 32;
  }
}



// Function to handle one MS Drive...
void procesMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc)
{
  Serial.printf("Initialize USB drive...");
  cmsReport = 0;
  if (!msc.begin(&msDrive)) {
    Serial.println("");
    msc.errorPrint(&Serial);
    Serial.printf("initialization drive %u failed.\n", drive_number);
  } else {
    Serial.printf("USB drive %u is present.\n", drive_number);
  }
  cmsReport = -1;

  //  mbrDmp( &msc );
  mbrDmp( msc.usbDrive() );

  for (uint8_t i = 1; i < 5; i++) {
    PFsVolume partVol;
    uint8_t volName[32];
    if (!partVol.begin(msc.usbDrive(), true, i)) continue; // not a valid volume.
    partVol.chvol();

    switch (partVol.fatType())
    {
      case FAT_TYPE_FAT12: Serial.print("\n>> Fat12: "); break;
      case FAT_TYPE_FAT16: Serial.print("\n>> Fat16: "); break;
      case FAT_TYPE_FAT32: Serial.print("\n>> Fat32: "); break;
      case FAT_TYPE_EXFAT: Serial.print("\n>> ExFat: "); break;
    }
    if (getPartitionVolumeLabel(partVol, volName, sizeof(volName))) {
      Serial.printf("Volume name:(%s)", volName);
    }
    elapsedMicros em_sizes = 0;
    uint64_t used_size =  (uint64_t)(partVol.clusterCount() - partVol.freeClusterCount())
                          * (uint64_t)partVol.bytesPerCluster();
    uint64_t total_size = (uint64_t)partVol.clusterCount() * (uint64_t)partVol.bytesPerCluster();
    Serial.printf(" Partition Total Size:%llu Used:%llu time us: %u\n", total_size, used_size, (uint32_t)em_sizes);

    partVol.ls();
  }
}

//================================================================
void loop(void) {
  //--------------------------------------------------
  cmsReport = 0;
  myusb.Task();
  if (!msDrive1) {
    Serial.println("Waiting up to 5 seconds for USB drive 1");
    elapsedMillis em = 0;
    while (!msDrive1 && (em < 5000) )  myusb.Task();
  }
  if (!msDrive2) {
    Serial.println("Waiting up to 5 seconds for USB drive 2");
    elapsedMillis em = 0;
    while (!msDrive2 && (em < 5000) )  myusb.Task();
  }
  if (msDrive1) {
    procesMSDrive(1, msDrive1, msc1);
  }
  if (msDrive2) {
    procesMSDrive(2, msDrive2, msc2);
  }
  cmsReport = -1;
  //--------------------------------------------------
  Serial.printf("\nInitialize SD card...");

  if (!sd.begin(SD_CONFIG)) {
    Serial.println("initialization failed.\n");
  } else {
    Serial.println("SD card is present.\n");
    mbrDmp(sd.card() );
    PFsVolume partVol;

    for (uint8_t i = 1; i < 5; i++) {
      PFsVolume partVol;
      uint8_t volName[32];
      if (!partVol.begin(sd.card(), true, i)) continue; // not a valid volume.
      partVol.chvol();

      switch (partVol.fatType())
      {
        case FAT_TYPE_FAT12: Serial.print("\n>> Fat12: "); break;
        case FAT_TYPE_FAT16: Serial.print("\n>> Fat16: "); break;
        case FAT_TYPE_FAT32: Serial.print("\n>> Fat32: "); break;
        case FAT_TYPE_EXFAT: Serial.print("\n>> ExFat: "); break;
      }
      if (getPartitionVolumeLabel(partVol, volName, sizeof(volName))) {
        Serial.printf("Volume name:(%s)", volName);
      }
      elapsedMicros em_sizes = 0;
      uint64_t used_size =  (uint64_t)(partVol.clusterCount() - partVol.freeClusterCount())
                            * (uint64_t)partVol.bytesPerCluster();
      uint64_t total_size = (uint64_t)partVol.clusterCount() * (uint64_t)partVol.bytesPerCluster();
      Serial.printf(" Partition Total Size:%llu Used:%llu time us: %u\n", total_size, used_size, (uint32_t)em_sizes);

      partVol.ls();
    }
  }

  Serial.println("done...");

  Serial.println("Press any key to run again");
  while (Serial.read() == -1);
  while (Serial.read() != -1);
}