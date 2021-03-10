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



typedef struct {
  uint32_t free;
  uint32_t todo;
  uint32_t clusters_per_sector;
} _gfcc_t;


void _getfreeclustercountCB(uint32_t token, uint8_t *buffer) 
{
  digitalWriteFast(1, HIGH);
  _gfcc_t *gfcc = (_gfcc_t *)token;
  uint16_t cnt = gfcc->clusters_per_sector;
  if (cnt > gfcc->todo) cnt = gfcc->todo;
  gfcc->todo -= cnt; // update count here...

  if (gfcc->clusters_per_sector == 512/2) {
    // fat16
    uint16_t *fat16 = (uint16_t *)buffer;
    while (cnt-- ) {
      if (*fat16++ == 0) gfcc->free++;
    }
  } else {
    uint32_t *fat32 = (uint32_t *)buffer;
    while (cnt-- ) {
      if (*fat32++ == 0) gfcc->free++;
    }
  }

  digitalWriteFast(1, LOW);
}

//-------------------------------------------------------------------------------------------------
uint32_t GetFreeClusterCount(USBmscInterface *usmsci, PFsVolume &partVol)
{

  FatVolume* fatvol =  partVol.getFatVol();
  if (!fatvol) return 0;

  _gfcc_t gfcc; 
  gfcc.free = 0;

  switch (partVol.fatType()) {
    default: return 0;
    case FAT_TYPE_FAT16: gfcc.clusters_per_sector = 512/2; break;
    case FAT_TYPE_FAT32: gfcc.clusters_per_sector = 512/4; break;
  }
  gfcc.todo = fatvol->clusterCount() + 2;

  digitalWriteFast(0, HIGH);
  usmsci->readSectorsWithCB(fatvol->fatStartSector(), gfcc.todo / gfcc.clusters_per_sector + 1, 
      &_getfreeclustercountCB, (uint32_t)&gfcc);
  digitalWriteFast(0, LOW);

  return gfcc.free;
}


//-------------------------------------------------------------------------------------------------

bool mbrDmpExtended(BlockDeviceInterface *blockDev, uint32_t sector, uint8_t indent) {
  MbrSector_t mbr;
  // bool valid = true;
  if (!blockDev->readSector(sector, (uint8_t*)&mbr)) {
    Serial.print("\nread extended MBR failed.\n");
    //errorPrint();
    return false;
  }
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
    //    if ((pt->boot != 0 && pt->boot != 0X80) ||
    //        getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
    //      valid = false;
    //    }
    for (uint8_t ii=0; ii< indent; ii++)Serial.write('\t');
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

    // for fun of it... try printing extended data
    if (pt->type == 0xf) mbrDmpExtended(blockDev, starting_sector, indent+1);
  }
  return true;
}
//-------------------------------------------------------------------------------------------------

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

    // for fun of it... try printing extended data
    if (pt->type == 0xf) mbrDmpExtended(blockDev, starting_sector, 1);
  }
  return true;
}

void setup()
{
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

void dump_hexbytes(const void *ptr, int len)
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

  #if 1
  bool partition_valid[4];
  PFsVolume partVol[4];
  char volName[32];

  for (uint8_t i = 0; i < 4; i++) {
    partition_valid[i] = partVol[i].begin((USBMSCDevice*)msc.usbDrive(), true, i+1);
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
      if (partVol[i].getVolumeLabel(volName, sizeof(volName))) {
        Serial.printf("Volume name:(%s)", volName);
      }
      elapsedMicros em_sizes = 0;
      uint32_t free_cluster_count = partVol[i].freeClusterCount();
      uint64_t used_size =  (uint64_t)(partVol[i].clusterCount() - free_cluster_count)
                            * (uint64_t)partVol[i].bytesPerCluster();
      uint64_t total_size = (uint64_t)partVol[i].clusterCount() * (uint64_t)partVol[i].bytesPerCluster();
      Serial.printf(" Partition Total Size:%llu Used:%llu time us: %u\n", total_size, used_size, (uint32_t)em_sizes);

      em_sizes = 0; // lets see how long this one takes. 
      uint32_t free_clusters_fast = GetFreeClusterCount(msc.usbDrive(), partVol[i]);
      Serial.printf("    Free Clusters: API: %u by CB:%u time us: %u\n", free_cluster_count, free_clusters_fast, (uint32_t)em_sizes);
      
      em_sizes = 0; // lets see how long this one takes. 
      uint32_t free_clusters_info = partVol[i].getFSInfoSectorFreeClusterCount();
      Serial.printf("    Free Clusters: Info: %u time us: %u\n", free_clusters_info, (uint32_t)em_sizes);


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
      char volName[32];
      if (!partVol.begin(sd.card(), true, i)) continue; // not a valid volume.
      partVol.chvol();

      switch (partVol.fatType())
      {
        case FAT_TYPE_FAT12: Serial.print("\n>> Fat12: "); break;
        case FAT_TYPE_FAT16: Serial.print("\n>> Fat16: "); break;
        case FAT_TYPE_FAT32: Serial.print("\n>> Fat32: "); break;
        case FAT_TYPE_EXFAT: Serial.print("\n>> ExFat: "); break;
      }
      if (partVol.getVolumeLabel(volName, sizeof(volName))) {
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