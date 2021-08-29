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

PFsLib pfsLIB;

uint8_t  sectorBuffer[512];
uint8_t volName[32];

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
  pfsLIB.mbrDmp( msc.usbDrive(),  msDrive.msCapacity.Blocks, Serial);

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
  pfsLIB.mbrDmp(sd.card(), (uint32_t)-1 , Serial);
//  PFsVolume partVol;

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
  pfsLIB.mbrDmp(sdSPI.card(), (uint32_t)-1 , Serial);
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

//----------------------------------------------------------------
// Function to handle one MS Drive...
//----------------------------------------------------------------

void CreatePartition(uint8_t drive_index, uint32_t formatType, uint32_t starting_sector, uint32_t count_of_sectors) 
{
    // What validation should we do here?  Could do a little like if 0 for count 
    // find out how big the drive is...
  if (!msDrives[drive_index]) {
    Serial.println("Not a valid USB drive");
    return;
  }
  if(formatType == 64) {
    pfsLIB.createExFatPartition(msc[drive_index].usbDrive(), starting_sector, count_of_sectors, sectorBuffer, &Serial);
  } else {
    pfsLIB.createFatPartition(msc[drive_index].usbDrive(), formatType, starting_sector, count_of_sectors, sectorBuffer, &Serial);
  }
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

  pfsLIB.InitializeDrive(dev, fat_type, &Serial);

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
        pfsLIB.formatter(partVols[partVol_index], fat_type, false, g_exfat_dump_changed_sectors, Serial); 
      break;

    case 'd':
      Serial.printf("\n **** Start dump partition %d ****\n", partVol_index);
      if (partVol_index < count_partVols) 
        pfsLIB.formatter(partVols[partVol_index], 0, true, g_exfat_dump_changed_sectors, Serial); 
      break;
    case 'p':
      while (partVol_index < count_partVols) {
        Serial.printf("\n **** print partition info %d ****\n", partVol_index);
        pfsLIB.print_partion_info(partVols[partVol_index], Serial); 
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
        uint32_t formatType      = CommandLineReadNextNumber(ch, 0);  //16, 32, 64(exFat)
        uint32_t starting_sector = CommandLineReadNextNumber(ch, 0);
        uint32_t count_of_sectors = CommandLineReadNextNumber(ch, 0);
        Serial.printf("\n *** Create a NEW partition Drive %u Starting at: %u Count: %u ***\n", partVol_index, starting_sector, count_of_sectors);
        CreatePartition(partVol_index, formatType, starting_sector, count_of_sectors);
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
          if (usb_device_index < CNT_MSDRIVES) pfsLIB.deletePartition(msc[usb_device_index].usbDrive(), partVol_index, &Serial, Serial);
          else Serial.println("Drive index is out of range");
        } else {
          if (partVol_index < count_partVols) {
            pfsLIB.deletePartition(partVols[partVol_index].blockDevice(), partVols[partVol_index].part(), &Serial, Serial);
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