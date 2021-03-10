#include "Arduino.h"
#include "mscFS.h"

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

#define SD_DRIVE 1
#define MS_DRIVE 2

#define SD_CONFIG SdioConfig(FIFO_SDIO)

// set up variables using the mscFS utility library functions:
UsbFs msc1;
UsbFs msc2;

SdFs sd;
PFsFatFormatter FatFormatter;
uint8_t  sectorBuffer[512];


bool getPartitionVolumeLabel(PFsVolume &partVol, uint8_t *pszVolName, uint16_t cb) {
  uint8_t buf[512];
  if (!pszVolName || (cb < 12)) return false; // don't want to deal with it

  PFsFile root;
  if (!root.openRoot(&partVol)) return false;
  root.read(buf, 32);
  //dump_hexbytes(buf, 32);

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

//----------------------------------------------------------------
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
        case 0xf:
        Serial.print("Extend:\t");
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
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    Serial.print(starting_sector, DEC); Serial.print(',');
    Serial.println(getLe32(pt->totalSectors));
  }
  return true;
}

//----------------------------------------------------------------

// Function to handle one MS Drive...
void procesMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc)
{
  Serial.printf("Initialize USB drive...");
  if (!msc.begin(&msDrive)) {
    Serial.println("");
    msc.errorPrint(&Serial);
    Serial.printf("initialization drive %u failed.\n", drive_number);
  } else {
    Serial.printf("USB drive %u is present.\n", drive_number);
  }

  //  mbrDmp( &msc );
  mbrDmp( msc.usbDrive() );

  #if 1
  bool partition_valid[4];
  PFsVolume partVol[4];
  uint8_t volName[32];

  for (uint8_t i = 0; i < 4; i++) {
    partition_valid[i] = partVol[i].begin(msc.usbDrive(), true, i+1);
    Serial.printf("Partition %u valid:%u\n", i, partition_valid[i]);
  }
  for (uint8_t i = 0; i < 4; i++) {
    if(partition_valid[i]) {
      switch (partVol[i].fatType())
      {
        case FAT_TYPE_FAT12: Serial.printf("%d:>> Fat12: ", i); break;
        case FAT_TYPE_FAT16: Serial.printf("%d:>> Fat16: ", i); break;
        case FAT_TYPE_FAT32: Serial.printf("%d:>> Fat32: ", i); break;
        case FAT_TYPE_EXFAT: Serial.printf("%d:>> ExFat: ", i); break;
      }
      if (getPartitionVolumeLabel(partVol[i], volName, sizeof(volName))) {
        Serial.printf("Volume name:(%s)", volName);
      }
      elapsedMicros em_sizes = 0;
      uint32_t free_cluster_count = partVol[i].freeClusterCount();
      uint64_t used_size =  (uint64_t)(partVol[i].clusterCount() - free_cluster_count)
                            * (uint64_t)partVol[i].bytesPerCluster();
      uint64_t total_size = (uint64_t)partVol[i].clusterCount() * (uint64_t)partVol[i].bytesPerCluster();
      Serial.printf(" Partition Total Size:%llu Used:%llu time us: %u\n", total_size, used_size, (uint32_t)em_sizes);

      partVol[i].ls();
    }
  }
  #else
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
  #endif
}  

//----------------------------------------------------------------

// Function to handle one MS Drive...
void formatter(uint8_t drive_number, msController &msDrive, UsbFs &msc, uint8_t part)
{
  Serial.printf("Initialize USB drive...");
  if (!msc.begin(&msDrive)) {
    Serial.println("");
    msc.errorPrint(&Serial);
    Serial.printf("initialization drive %u failed.\n", drive_number);
  } else {
    Serial.printf("USB drive %u is present.\n", drive_number);
  }

  //mbrDmp( msc.usbDrive() );

  PFsVolume partVol;
  uint8_t volName[32];
  bool partition_valid;

    partition_valid = partVol.begin(msc.usbDrive(), true, part+1);
    Serial.printf("Partition %u valid:%u\n", part, partition_valid);

  if(partition_valid)
  FatFormatter.format(msc.usbDrive(), part, partVol, sectorBuffer, &Serial);
  else
  Serial.println("Cannot format an invalid partition");
  
}
//----------------------------------------------------------------

void setup() {
#if 0 // easy test to check HardFault Detection response
  int *pp = 0;
  *pp = 5;
#endif
pinMode(0, OUTPUT);
pinMode(1, OUTPUT);
pinMode(2, OUTPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

}


void loop() {

  myusb.Task();
  if (!msDrive1) {
    Serial.println("Waiting up to 5 seconds for USB drive 1");
    elapsedMillis em = 0;
    while (!msDrive1 && (em < 5000) )  myusb.Task();
  }

  if (msDrive1) {
    procesMSDrive(1, msDrive1, msc1);
  }

  Serial.println("done...");
  Serial.println("Enter 0, 1 , 2, or 3 for Partition or Enter to Bypass");
  char temp = '0';
  while ( !Serial.available() );
  while ( Serial.available() ) {
    temp = Serial.read();
  switch(temp) {
    case('0'):
      //drive 1, , , partition 0-3
      formatter(1, msDrive1, msc1, 0);
      break;
    case('1'):
      //drive 1, , , partition 0-3
      formatter(1, msDrive1, msc1, 1);
      break;
    case('2'):
      //drive 1, , , partition 0-3
      //formatter(1, msDrive1, msc1, 2);
      break;
    case('3'):
      //drive 1, , , partition 0-3
      formatter(1, msDrive1, msc1, 3);
      break;
    default:
      break;
  }
  }


  Serial.println("Press any key to run again");
  while (Serial.read() == -1);
  while (Serial.read() != -1);
}
