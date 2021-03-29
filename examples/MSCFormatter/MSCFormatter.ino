#include "Arduino.h"
#include "mscFS.h"
#include "sdios.h"

// Serial output stream
ArduinoOutStream cout(Serial);

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);

// Setup MSC for the number of USB Drives you are using. (Two for this example)
// Mutiple  USB drives can be used. Hot plugging is supported. There is a slight
// delay after a USB MSC device is plugged in. This is waiting for initialization
// but after it is initialized ther should be no delay.
#define CNT_PARITIONS 10 
PFsVolume partVols[CNT_PARITIONS];
uint8_t partVols_drive_index[CNT_PARITIONS];
uint8_t count_partVols = 0;
  
#define CNT_MSDRIVES 3
msController msDrives[CNT_MSDRIVES](myusb);
UsbFs msc[CNT_MSDRIVES];
bool g_exfat_dump_changed_sectors = false;

#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_SPICONFIG SdioConfig(FIFO_SDIO)


#define LOGICAL_DRIVE_SDIO 10
#define LOGICAL_DRIVE_SDSPI 11
SdFs sd;
SdFs sdSPI;
#define SD_SPI_CS 10
#define SPI_SPEED SD_SCK_MHZ(33)  // adjust to sd card 

PFsFatFormatter FatFormatter;
PFsExFatFormatter ExFatFormatter;
uint8_t  sectorBuffer[512];
uint8_t volName[32];

//----------------------------------------------------------------
uint32_t mbrDmp(BlockDeviceInterface *blockDev, uint32_t device_sector_count) {
  MbrSector_t mbr;
  uint32_t next_free_sector = 8192;  // Some inital value this is default for Win32 on SD...
  // bool valid = true;
  if (!blockDev->readSector(0, (uint8_t*)&mbr)) {
    Serial.print("\nread MBR failed.\n");
    //errorPrint();
    return (uint32_t)-1;
  }
  Serial.print("\nmsc # Partition Table\n");
  Serial.print("\tpart,boot,bgnCHS[3],type,endCHS[3],start,length\n");
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    uint32_t total_sector = getLe32(pt->totalSectors);
    if (starting_sector > next_free_sector) {
      Serial.printf("\t < unused area starting at: %u length %u >\n", next_free_sector, starting_sector-next_free_sector);
    }
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
    case 0x83: Serial.print("ext2/3/4:\t"); break; 
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
    Serial.print(starting_sector, DEC); Serial.print(',');
    Serial.println(total_sector);

    // Lets get the max of start+total
    if (starting_sector && total_sector)  next_free_sector = starting_sector + total_sector;
  }
  if ((device_sector_count != (uint32_t)-1) && (next_free_sector < device_sector_count)) {
    Serial.printf("\t < unused area starting at: %u length %u >\n", next_free_sector, device_sector_count-next_free_sector);
  } 
  return next_free_sector;
}
//----------------------------------------------------------------

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
//----------------------------------------------------------------

void compare_dump_hexbytes(const void *ptr, const uint8_t *compare_buf, int len)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%c%02X", (p[i]==compare_buf[i])? ' ' : '*',p[i]);
    }
    Serial.print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    Serial.println();
    p += 32;
    compare_buf += 32;
    len -= 32;
  }
}




//----------------------------------------------------------------

// Function to handle one MS Drive...
void processMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc)
{
  Serial.printf("Initialize USB drive...");
  //cmsReport = 0;
  if (!msc.begin(&msDrive)) {
    Serial.println("");
    msc.errorPrint(&Serial);
    Serial.printf("initialization drive %u failed.\n", drive_number);

    // see if we can read capacity of this device
    msSCSICapacity_t mscap;
    uint8_t status = msDrive.msReadDeviceCapacity(&mscap);
    Serial.printf("Read Capacity: status: %u Blocks: %u block Size: %u\n", status, mscap.Blocks, mscap.BlockSize);
    Serial.printf("From object: Blocks: %u Size: %u\n", msDrive.msCapacity.Blocks, msDrive.msCapacity.BlockSize);
    return;
  }
  Serial.printf("Blocks: %u Size: %u\n", msDrive.msCapacity.Blocks, msDrive.msCapacity.BlockSize);
  mbrDmp( msc.usbDrive(),  msDrive.msCapacity.Blocks);

  // lets see if we have any partitions to add to our list...
  for (uint8_t i = 0; i < 4; i++) {
    if (count_partVols == CNT_PARITIONS) return; // don't overrun
    if (partVols[count_partVols].begin((USBMSCDevice*)msc.usbDrive(), true, i + 1)) {
      Serial.printf("drive %u Partition %u valid:%u\n", drive_number, i);
      partVols_drive_index[count_partVols] = drive_number;

      count_partVols++;
    }
  }
}

//----------------------------------------------------------------
// Function to handle one MS Drive...
void processSDDrive()
{
    Serial.printf("\nInitialize SDIO SD card...");

  if (!sd.begin(SD_CONFIG)) {
    Serial.println("initialization failed.\n");
    return;
  }
  mbrDmp(sd.card(), (uint32_t)-1 );
  PFsVolume partVol;

  for (uint8_t i = 0; i < 4; i++) {
  if (count_partVols == CNT_PARITIONS) return; // don't overrun
    if (partVols[count_partVols].begin(sd.card(), true, i + 1)) {
      Serial.printf("drive s Partition %u valid\n", i);
      partVols_drive_index[count_partVols] = LOGICAL_DRIVE_SDIO;
      count_partVols++;
    }
  }
}

void ProcessSPISD() {
    Serial.printf("\nInitialize SPI SD card...");

  if(!sdSPI.begin(SdSpiConfig(SD_SPI_CS, SHARED_SPI, SPI_SPEED))) {
    Serial.println("initialization failed.\n");
    return;
  }
  mbrDmp(sdSPI.card(), (uint32_t)-1 );
  for (uint8_t i = 0; i < 4; i++) {
  if (count_partVols == CNT_PARITIONS) return; // don't overrun
    if (partVols[count_partVols].begin(sdSPI.card(), true, i + 1)) {
      partVols_drive_index[count_partVols] = LOGICAL_DRIVE_SDSPI;
      Serial.printf("drive g Partition %u valid\n", i);
      count_partVols++;
    }
  }
}


void ShowPartitionList() {
  Serial.println("\n***** Partition List *****");
  char volName[32];
  for (uint8_t i = 0; i < count_partVols; i++)  {
    Serial.printf("%d(%u:%u):>> ", i, partVols_drive_index[i], partVols[i].part());
    switch (partVols[i].fatType())
    {
    case FAT_TYPE_FAT12: Serial.printf("Fat12: "); break;
    case FAT_TYPE_FAT16: Serial.printf("Fat16: "); break;
    case FAT_TYPE_FAT32: Serial.printf("Fat32: "); break;
    case FAT_TYPE_EXFAT: Serial.printf("ExFat: "); break;
    }
    if (partVols[i].getVolumeLabel(volName, sizeof(volName))) {
      Serial.printf("Volume name:(%s)", volName);
    }
    elapsedMicros em_sizes = 0;
    uint32_t free_cluster_count = partVols[i].freeClusterCount();
    uint64_t used_size =  (uint64_t)(partVols[i].clusterCount() - free_cluster_count)
                          * (uint64_t)partVols[i].bytesPerCluster();
    uint64_t total_size = (uint64_t)partVols[i].clusterCount() * (uint64_t)partVols[i].bytesPerCluster();
    Serial.printf(" Partition Total Size:%llu Used:%llu time us: %u\n", total_size, used_size, (uint32_t)em_sizes);
  }
}

void print_partion_info(PFsVolume &partVol) 
{
  uint8_t buffer[512];
  MbrSector_t *mbr = (MbrSector_t *)buffer;
  if (!partVol.blockDevice()->readSector(0, buffer)) return;
  MbrPart_t *pt = &mbr->part[partVol.part() - 1];

  uint32_t starting_sector = getLe32(pt->relativeSectors);
  uint32_t sector_count = getLe32(pt->totalSectors);
  Serial.printf("Starting Sector: %u, Sector Count: %u\n", starting_sector, sector_count);    

  FatPartition *pfp = partVol.getFatVol();
  if (pfp) {
    Serial.printf("fatType:%u\n", pfp->fatType());
    Serial.printf("bytesPerClusterShift:%u\n", pfp->bytesPerClusterShift());
    Serial.printf("bytesPerCluster:%u\n", pfp->bytesPerCluster());
    Serial.printf("bytesPerSector:%u\n", pfp->bytesPerSector());
    Serial.printf("bytesPerSectorShift:%u\n", pfp->bytesPerSectorShift());
    Serial.printf("sectorMask:%u\n", pfp->sectorMask());
    Serial.printf("sectorsPerCluster:%u\n", pfp->sectorsPerCluster());
    Serial.printf("sectorsPerFat:%u\n", pfp->sectorsPerFat());
    Serial.printf("clusterCount:%u\n", pfp->clusterCount());
    Serial.printf("dataStartSector:%u\n", pfp->dataStartSector());
    Serial.printf("fatStartSector:%u\n", pfp->fatStartSector());
    Serial.printf("rootDirEntryCount:%u\n", pfp->rootDirEntryCount());
    Serial.printf("rootDirStart:%u\n", pfp->rootDirStart());
  }
} 

//----------------------------------------------------------------
// Function to handle one MS Drive...
void formatter(PFsVolume &partVol, uint8_t fat_type, bool dump_drive)
{

  if (fat_type == 0) fat_type = partVol.fatType();

  if (fat_type != FAT_TYPE_FAT12) {
    // 
    uint8_t buffer[512];
    MbrSector_t *mbr = (MbrSector_t *)buffer;
    if (!partVol.blockDevice()->readSector(0, buffer)) return;
    MbrPart_t *pt = &mbr->part[partVol.part() - 1];

    uint32_t sector = getLe32(pt->relativeSectors);

    // I am going to read in 24 sectors for EXFat.. 
    uint8_t *bpb_area = (uint8_t*)malloc(512*24); 
    if (!bpb_area) {
      Serial.println("Unable to allocate dump memory");
      return;
    }
    // Lets just read in the top 24 sectors;
    uint8_t *sector_buffer = bpb_area;
    for (uint32_t i = 0; i < 24; i++) {
      partVol.blockDevice()->readSector(sector+i, sector_buffer);
      sector_buffer += 512;
    }

    if (dump_drive) {
      sector_buffer = bpb_area;
      
      for (uint32_t i = 0; i < 12; i++) {
        Serial.printf("\nSector %u(%u)\n", i, sector);
        dump_hexbytes(sector_buffer, 512);
        sector++;
        sector_buffer += 512;
      }
      for (uint32_t i = 12; i < 24; i++) {
        Serial.printf("\nSector %u(%u)\n", i, sector);
        compare_dump_hexbytes(sector_buffer, sector_buffer - (512*12), 512);
        sector++;
        sector_buffer += 512;
      }

    } else {  
      if (fat_type != FAT_TYPE_EXFAT) {
        FatFormatter.format(partVol, fat_type, sectorBuffer, &Serial);
      } else {
        Serial.println("ExFatFormatter - WIP");
        ExFatFormatter.format(partVol, sectorBuffer, &Serial);
        if (g_exfat_dump_changed_sectors) {
          // Now lets see what changed
          uint8_t *sector_buffer = bpb_area;
          for (uint32_t i = 0; i < 24; i++) {
            partVol.blockDevice()->readSector(sector, buffer);
            Serial.printf("Sector %u(%u)\n", i, sector);
            if (memcmp(buffer, sector_buffer, 512)) {
              compare_dump_hexbytes(buffer, sector_buffer, 512);
              Serial.println();
            }
            sector++;
            sector_buffer += 512;
          }
        }
      }
    }
    free(bpb_area); 
  }
  else
    Serial.println("Cannot format an invalid partition");
}


//----------------------------------------------------------------
#define SECTORS_2GB 4194304   // (2^30 * 2) / 512
#define SECTORS_32GB 67108864 // (2^30 * 32) / 512
#define SECTORS_127GB 266338304 // (2^30 * 32) / 512

void CreatePartition(uint8_t drive_index, uint32_t starting_sector, uint32_t count_of_sectors) 
{
    // What validation should we do here?  Could do a little like if 0 for count 
    // find out how big the drive is...
  if (!msDrives[drive_index]) {
    Serial.println("Not a valid USB drive");
    return;
  }
  FatFormatter.createPartition(msc[drive_index].usbDrive(), 0, starting_sector, count_of_sectors, sectorBuffer, &Serial);

}

//----------------------------------------------------------------
// Function to handle one MS Drive...
void InitializeBlockDevice(uint8_t drive_index, uint8_t fat_type)
{
  BlockDeviceInterface *dev = nullptr;
  PFsVolume partVol;

  for (int ii = 0; ii < count_partVols; ii++) {
    if (partVols_drive_index[ii] == drive_index) {
      while (Serial.read() != -1) ;
      Serial.println("Warning it appears like this drive has valid partitions, continue: Y? ");
      int ch;
      while ((ch = Serial.read()) == -1) ;
      if (ch != 'Y') {
        Serial.println("Canceled");
        return;
      }
      break;
    }
  }

  if (drive_index == LOGICAL_DRIVE_SDIO) {
    dev = sd.card();
  } else if (drive_index == LOGICAL_DRIVE_SDSPI) {
    dev = sdSPI.card();
  } else {
    if (!msDrives[drive_index]) {
      Serial.println("Not a valid USB drive");
      return;
    }
    dev = (USBMSCDevice*)msc[drive_index].usbDrive();
  }

  uint32_t sectorCount = dev->sectorCount();
  
  // Serial.printf("Blocks: %u Size: %u\n", msDrives[drive_index].msCapacity.Blocks, msDrives[drive_index].msCapacity.BlockSize);
  if ((fat_type == FAT_TYPE_EXFAT) && (sectorCount < 0X100000 )) fat_type = 0; // hack to handle later
  if ((fat_type == FAT_TYPE_FAT16) && (sectorCount >= SECTORS_2GB )) fat_type = 0; // hack to handle later
  if ((fat_type == FAT_TYPE_FAT32) && (sectorCount >= SECTORS_127GB )) fat_type = 0; // hack to handle later
  if (fat_type == 0)  {
    // assume 512 byte blocks here.. 
    if (sectorCount < SECTORS_2GB) fat_type = FAT_TYPE_FAT16;
    else if (sectorCount < SECTORS_32GB) fat_type = FAT_TYPE_FAT32;
    else fat_type = FAT_TYPE_EXFAT;
  }
  // lets generate a MBR for this type...
  memset(sectorBuffer, 0, 512); // lets clear out the area.
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(sectorBuffer);

  // make Master Boot Record.  Use fake CHS.
  // fill in common stuff. 
  mbr->part->beginCHS[0] = 1;
  mbr->part->beginCHS[1] = 1;
  mbr->part->beginCHS[2] = 0;
  mbr->part->type = 7;
  mbr->part->endCHS[0] = 0XFE;
  mbr->part->endCHS[1] = 0XFF;
  mbr->part->endCHS[2] = 0XFF;
  setLe16(mbr->signature, MBR_SIGNATURE);


  if (fat_type == FAT_TYPE_EXFAT) {
    uint32_t clusterCount;
    uint32_t clusterHeapOffset;
    uint32_t fatLength;
    uint32_t m;
    uint32_t partitionOffset;
    uint32_t volumeLength;
    uint8_t sectorsPerClusterShift;
    uint8_t vs;
    // Determine partition layout.
    for (m = 1, vs = 0; m && sectorCount > m; m <<= 1, vs++) {}
    sectorsPerClusterShift = vs < 29 ? 8 : (vs - 11)/2;
    fatLength = 1UL << (vs < 27 ? 13 : (vs + 1)/2);
    partitionOffset = 2*fatLength;
    clusterHeapOffset = 2*fatLength;
    clusterCount = (sectorCount - 4*fatLength) >> sectorsPerClusterShift;
    volumeLength = clusterHeapOffset + (clusterCount << sectorsPerClusterShift);

    setLe32(mbr->part->relativeSectors, partitionOffset);
    setLe32(mbr->part->totalSectors, volumeLength);
  
  } else {
    // Fat16 or fat32...
    uint16_t const BU16 = 128;
    uint16_t const BU32 = 8192;
    // Assume 512 byte sectors.
    const uint16_t BYTES_PER_SECTOR = 512;
    const uint16_t SECTORS_PER_MB = 0X100000/BYTES_PER_SECTOR;
    const uint16_t FAT16_ROOT_ENTRY_COUNT = 512;
    const uint16_t FAT16_ROOT_SECTOR_COUNT = 32*FAT16_ROOT_ENTRY_COUNT/BYTES_PER_SECTOR;

    uint32_t capacityMB = (sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;
    uint32_t sectorsPerCluster = 0;
    uint32_t nc;
    uint32_t r;
    uint32_t dataStart;
    uint32_t fatSize;
    uint32_t reservedSectorCount;
    uint32_t relativeSectors;
    uint32_t totalSectors;
    uint8_t partType;

    if (capacityMB <= 6) {
      Serial.print("Card is too small.\r\n");
      return;
    } else if (capacityMB <= 16) {
      sectorsPerCluster = 2;
    } else if (capacityMB <= 32) {
      sectorsPerCluster = 4;
    } else if (capacityMB <= 64) {
      sectorsPerCluster = 8;
    } else if (capacityMB <= 128) {
      sectorsPerCluster = 16;
    } else if (capacityMB <= 1024) {
      sectorsPerCluster = 32;
    } else if (capacityMB <= 32768) {
      sectorsPerCluster = 64;
    } else {
      // SDXC cards
      sectorsPerCluster = 128;
    }

    // Fat16
    if (fat_type == FAT_TYPE_FAT16) {
  
      for (dataStart = 2*BU16; ; dataStart += BU16) {
        nc = (sectorCount - dataStart)/sectorsPerCluster;
        fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
        r = BU16 + 1 + 2*fatSize + FAT16_ROOT_SECTOR_COUNT;
        if (dataStart >= r) {
          relativeSectors = dataStart - r + BU16;
          break;
        }
      }
      reservedSectorCount = 1;
      totalSectors = nc*sectorsPerCluster
                       + 2*fatSize + reservedSectorCount + 32;
      if (totalSectors < 65536) {
        partType = 0X04;
      } else {
        partType = 0X06;
      }
    } else {
      // fat32...
      relativeSectors = BU32;
      for (dataStart = 2*BU32; ; dataStart += BU32) {
        nc = (sectorCount - dataStart)/sectorsPerCluster;
        fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
        r = relativeSectors + 9 + 2*fatSize;
        if (dataStart >= r) {
          break;
        }
      }
      reservedSectorCount = dataStart - relativeSectors - 2*fatSize;
      totalSectors = nc*sectorsPerCluster + dataStart - relativeSectors;
      // type depends on address of end sector
      // max CHS has lba = 16450560 = 1024*255*63
      if ((relativeSectors + totalSectors) <= 16450560) {
        // FAT32 with CHS and LBA
        partType = 0X0B;
      } else {
        // FAT32 with only LBA
        partType = 0X0C;
      }
    }
    mbr->part->type = partType;
    setLe32(mbr->part->relativeSectors, relativeSectors);
    setLe32(mbr->part->totalSectors, totalSectors);
  }

  // Bugbug:: we assume that the msc is already set for this...
  dev->writeSector(0, sectorBuffer);

  // and lets setup a partition to use this area...
  partVol.begin(dev, 1); // blind faith it worked
    
  // now lets try calling formatter
  formatter(partVol, fat_type, false); 
 
}

//=============================================================================
bool DeletePartition(BlockDeviceInterface *blockDev, uint8_t part) 
{

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(sectorBuffer);
  if (!blockDev->readSector(0, sectorBuffer)) {
    Serial.print("\nERROR: read MBR failed.\n");
    return false;
  }

  if ((part < 1) || (part > 4)) {
    Serial.printf("ERROR: Invalid Partition: %u, only 1-4 are valid\n", part);
    return false;
  }

  Serial.println("Warning this will delete the partition are you sure, continue: Y? ");
  int ch;
  while ((ch = Serial.read()) == -1) ;
  if (ch != 'Y') {
    Serial.println("Canceled");
    return false;
  }
  Serial.println("MBR Before");
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));

  // Copy in the higher numer partitions; 
  for (--part; part < 3; part++)  memcpy(&mbr->part[part], &mbr->part[part+1], sizeof(MbrPart_t));
  // clear out the last one
  memset(&mbr->part[part], 0, sizeof(MbrPart_t));

  Serial.println("MBR After");
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));

  return blockDev->writeSector(0, sectorBuffer);
}




//=============================================================================
void setup() {
#if 0 // easy test to check HardFault Detection response
  int *pp = 0;
  *pp = 5;
#endif
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);

  // allow for SD on SPI
  SPI.begin();
  #if defined(SD_SPI_CS)
  pinMode(SD_SPI_CS, OUTPUT);
  digitalWriteFast(SD_SPI_CS, HIGH);
  #endif
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

  cout << F(
         "\n"
         "Cards up to 2 GiB (GiB = 2^30 bytes) will be formated FAT16.\n"
         "Cards larger than 2 GiB and up to 32 GiB will be formatted\n"
         "FAT32. Cards larger than 32 GiB will be formatted exFAT.\n"
         "\n");
    ShowCommandList();

}

//=============================================================================
void ShowCommandList() {
  Serial.println("Commands:");
  Serial.println("  f <partition> [16|32|ex] - to format");
  Serial.println("  v <partition> <label> - to change volume label");
  Serial.println("  d <partition> - to dump first sectors");
  Serial.println("  p <partition> - print Partition info");
  Serial.println("  l <partition> - to do ls command on that partition");
  Serial.println("  c -  toggle on/off format show changed data");  
  Serial.println("  *** Danger Zone ***");  
  Serial.println("  N <USB Device> start_addr <length> - Add a new partition to a disk");
  Serial.println("  R <USB Device> - Setup initial MBR and format disk *sledgehammer*");
  Serial.println("  X <partition> [d <usb device> - Delete a partition");
}

//=============================================================================
uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ') ch = Serial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = Serial.read(); 
  }
  return return_value;  
}


//=============================================================================
void loop() {

  myusb.Task();

  count_partVols = 0;

  if (!msDrives[0]) {
    Serial.println("Waiting up to 5 seconds for a USB drive ");
    elapsedMillis em = 0;
    while (em < 5000) {
      myusb.Task();
      for (uint8_t i = 0; i < CNT_MSDRIVES; i++) if (msDrives[i]) break;
    }
  }

  for (uint8_t i = 0; i < CNT_MSDRIVES; i++) {
    if (msDrives[i]) {
      processMSDrive(i, msDrives[i], msc[i]);
    }    
  }
  processSDDrive();
  ProcessSPISD();
  ShowPartitionList();

  Serial.println("done...");
  Serial.println("Enter command:");

  while ( !Serial.available() );
  uint8_t commmand = Serial.read();

  int ch = Serial.read();
  uint32_t partVol_index = CommandLineReadNextNumber(ch, 0);
  while (ch == ' ') ch = Serial.read();
  uint8_t fat_type = 0;
  switch(commmand) {
    default:
      ShowCommandList();
      break;      
    case 'f':
      // if there is an optional parameter try it... 
      switch(ch) {
        case '1': fat_type = FAT_TYPE_FAT16; break;
        case '3': fat_type = FAT_TYPE_FAT32; break;
        case 'e': fat_type = FAT_TYPE_EXFAT; break;
      }

      Serial.printf("\n **** Start format partition %d ****\n", partVol_index);
      if (partVol_index < count_partVols) 
        formatter(partVols[partVol_index], fat_type, false); 
      break;

    case 'd':
      Serial.printf("\n **** Start dump partition %d ****\n", partVol_index);
      if (partVol_index < count_partVols) 
        formatter(partVols[partVol_index], 0, true); 
      break;
    case 'p':
      while (partVol_index < count_partVols) {
        Serial.printf("\n **** print partition info %d ****\n", partVol_index);
        print_partion_info(partVols[partVol_index]); 
        partVol_index = (uint8_t)CommandLineReadNextNumber(ch, 0xff);
      }
      break;
    case 'v':
      {
      if (partVol_index < count_partVols) {
          char new_volume_name[30];
          int ii = 0;
          while (ch > ' ') {
            new_volume_name[ii++] = ch;
            if (ii == (sizeof(new_volume_name)-1)) break;
            ch = Serial.read();
          }
          new_volume_name[ii] = 0;
          Serial.printf("Try setting partition index %u to %s - ", partVol_index, new_volume_name);
          if (partVols[partVol_index].setVolumeLabel(new_volume_name)) Serial.println("*** Succeeded ***");
          else Serial.println("*** failed ***");
        }
      }
      break;
    case 'c': 
      g_exfat_dump_changed_sectors = !g_exfat_dump_changed_sectors; 
      break;
    case 'l':

      Serial.printf("\n **** List fillScreen partition %d ****\n", partVol_index);
      if (partVol_index < count_partVols) 
        partVols[partVol_index].ls();
      break;
    case 'N':
      {
      // do the work there...
        uint32_t starting_sector = CommandLineReadNextNumber(ch, 0);
        uint32_t count_of_sectors = CommandLineReadNextNumber(ch, 0);
        Serial.printf("\n *** Create a NEW partition Drive %u Starting at: %u Count: %u ***\n", partVol_index, starting_sector, count_of_sectors);
        CreatePartition(partVol_index, starting_sector, count_of_sectors);
      }

      break;  
    case 'R':
      Serial.printf("\n **** Try Sledgehammer on USB Drive %u ****\n", partVol_index);
      switch(ch) {
        case '1': fat_type = FAT_TYPE_FAT16; break;
        case '3': fat_type = FAT_TYPE_FAT32; break;
        case 'e': fat_type = FAT_TYPE_EXFAT; break;
      }
      InitializeBlockDevice(partVol_index, fat_type); 
      break;
    case 'X':
      {
        if (ch == 'd') {
          // User is using the devide version... 
          ch = Serial.read();             
          uint8_t usb_device_index = (uint8_t)CommandLineReadNextNumber(ch, 0);
          if (usb_device_index < CNT_MSDRIVES) DeletePartition(msc[usb_device_index].usbDrive(), partVol_index);
          else Serial.println("Drive index is out of range");
        } else {
          if (partVol_index < count_partVols) {
            DeletePartition(partVols[partVol_index].blockDevice(), partVols[partVol_index].part());
          } else {
            Serial.println("Partition index is out of range");
          }

        }
      }
  }
  while (Serial.read() != -1);

  Serial.println("Press any key to run again");
  while (Serial.read() == -1);
  while (Serial.read() != -1);
}
