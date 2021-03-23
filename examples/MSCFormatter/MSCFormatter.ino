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
bool g_exfat_dump_changed_sectors = true;

#define SD_DRIVE 1
#define MS_DRIVE 2
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_SPICONFIG SdioConfig(FIFO_SDIO)


SdFs sd;
SdFs sdSPI;
#define SD_SPI_CS 10
#define SPI_SPEED SD_SCK_MHZ(33)  // adjust to sd card 

PFsFatFormatter FatFormatter;
PFsExFatFormatter ExFatFormatter;
uint8_t  sectorBuffer[512];
uint8_t volName[32];

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
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    Serial.print(starting_sector, DEC); Serial.print(',');
    Serial.println(getLe32(pt->totalSectors));
  }
  return true;
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
    return;
  }
  mbrDmp( msc.usbDrive() );

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
  mbrDmp(sd.card() );
  PFsVolume partVol;

  for (uint8_t i = 0; i < 4; i++) {
  if (count_partVols == CNT_PARITIONS) return; // don't overrun
    if (partVols[count_partVols].begin(sd.card(), true, i + 1)) {
      Serial.printf("drive s Partition %u valid\n", i);
      partVols_drive_index[count_partVols] = 0xff;
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
  mbrDmp(sdSPI.card() );
  for (uint8_t i = 0; i < 4; i++) {
  if (count_partVols == CNT_PARITIONS) return; // don't overrun
    if (partVols[count_partVols].begin(sdSPI.card(), true, i + 1)) {
      partVols_drive_index[count_partVols] = 0xfe;
      Serial.printf("drive g Partition %u valid\n", i);
      count_partVols++;
    }
  }
}


void ShowPartitionList() {
  Serial.println("\n***** Partition List *****");
  char volName[32];
  for (uint8_t i = 0; i < count_partVols; i++)  {
    Serial.printf("%d(%x:%x):>> ", i, partVols_drive_index[i], partVols[i].part());
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
        FatFormatter.format(partVol, sectorBuffer, &Serial);
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

void setup() {
#if 0 // easy test to check HardFault Detection response
  int *pp = 0;
  *pp = 5;
#endif
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);

  // allow for SD on SPI...
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

}


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
  while (ch == ' ') ch = Serial.read();
  uint8_t partVol_index = 0;
  while ((ch >= '0') && (ch <= '9')) {
    partVol_index = partVol_index * 10 + ch - '0';
    ch = Serial.read(); 
  }
  while (ch == ' ') ch = Serial.read();
  uint8_t fat_type = 0;
  if (partVol_index < count_partVols) {
    switch(commmand) {
      default:
        Serial.println("Commands:");
        Serial.println("  f <partition> [16|32|ex] - to format");
        Serial.println("  v <partition> <label> - to change volume label");
        Serial.println("  d <partition> - to dump first sectors");
        Serial.println("  l <partition> - to do ls command on that partition");
        Serial.println("  c -  toggle on/off format show changed data");
        break;      
      case 'f':
        // if there is an optional parameter try it... 
        switch(ch) {
          case '1': fat_type = FAT_TYPE_FAT16; break;
          case '3': fat_type = FAT_TYPE_FAT32; break;
          case 'e': fat_type = FAT_TYPE_EXFAT; break;
        }

        Serial.printf("\n **** Start format partition %d ****\n", partVol_index);
        formatter(partVols[partVol_index], fat_type, false); 
        break;

      case 'd':
        Serial.printf("\n **** Start dump partition %d ****\n", partVol_index);
        formatter(partVols[partVol_index], 0, true); 
        break;
      case 'v':
        {
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
        break;
      case 'c': 
        g_exfat_dump_changed_sectors = !g_exfat_dump_changed_sectors; 
        break;
      case 'l':

        Serial.printf("\n **** List fillScreen partition %d ****\n", partVol_index);
        partVols[partVol_index].ls();
        break;

    }
  }
  while (Serial.read() != -1);

  Serial.println("Press any key to run again");
  while (Serial.read() == -1);
  while (Serial.read() != -1);
}
