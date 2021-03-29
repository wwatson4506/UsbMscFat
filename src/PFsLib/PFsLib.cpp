#include "PFsLib.h"

//Set to 0 for debug info
#define DBG_Print	1
#if defined(DBG_Print)
#define DBGPrintf Serial.printf
#else
void inline DBGPrintf(...) {};
#endif

//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
#elif defined(__AVR__)
#define writeMsg(str) if (m_pr) m_pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) if (m_pr) m_pr->write(str)
#endif  // PRINT_FORMAT_PROGRESS

//----------------------------------------------------------------
#define SECTORS_2GB 4194304   // (2^30 * 2) / 512
#define SECTORS_32GB 67108864 // (2^30 * 32) / 512
#define SECTORS_127GB 266338304 // (2^30 * 32) / 512

//uint8_t partVols_drive_index[10];

//=============================================================================
bool PFsLib::deletePartition(BlockDeviceInterface *blockDev, uint8_t part, print_t* pr) 
{
  uint8_t  sectorBuffer[512];

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(sectorBuffer);
  if (!blockDev->readSector(0, sectorBuffer)) {
    writeMsg("\nERROR: read MBR failed.\n");
    return false;
  }

  if ((part < 1) || (part > 4)) {
    Serial.printf("ERROR: Invalid Partition: %u, only 1-4 are valid\n", part);
    return false;
  }

  writeMsg("Warning this will delete the partition are you sure, continue: Y? ");
  int ch;
  while ((ch = Serial.read()) == -1) ;
  if (ch != 'Y') {
    writeMsg("Canceled");
    return false;
  }
  DBGPrintf("MBR Before");
#if(DBG_Print)
	dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));
#endif
  // Copy in the higher numer partitions; 
  for (--part; part < 3; part++)  memcpy(&mbr->part[part], &mbr->part[part+1], sizeof(MbrPart_t));
  // clear out the last one
  memset(&mbr->part[part], 0, sizeof(MbrPart_t));

  DBGPrintf("MBR After");
#if(DBG_Print)
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));
#endif
  return blockDev->writeSector(0, sectorBuffer);
}

//===========================================================================
//----------------------------------------------------------------
#define SECTORS_2GB 4194304   // (2^30 * 2) / 512
#define SECTORS_32GB 67108864 // (2^30 * 32) / 512
#define SECTORS_127GB 266338304 // (2^30 * 32) / 512

//uint8_t partVols_drive_index[10];

//----------------------------------------------------------------
// Function to handle one MS Drive...
//msc[drive_index].usbDrive()
void PFsLib::InitializeDrive(BlockDeviceInterface *dev, uint8_t fat_type, print_t* pr)
{
  uint8_t  sectorBuffer[512];

  m_dev = dev;
  
  
  //TODO: have to see if this is still valid
  PFsVolume partVol;
/*
  for (int ii = 0; ii < count_partVols; ii++) {
    if (partVols_drive_index[ii] == drive_index) {
      while (Serial.read() != -1) ;
      writeMsg("Warning it appears like this drive has valid partitions, continue: Y? ");
      int ch;
      while ((ch = Serial.read()) == -1) ;
      if (ch != 'Y') {
        writeMsg("Canceled");
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
      writeMsg("Not a valid USB drive");
      return;
    }
    dev = (USBMSCDevice*)msc[drive_index].usbDrive();
  }
*/
  uint32_t sectorCount = dev->sectorCount();
  
  Serial.printf("sectorCount = %u, FatType: %x\n", sectorCount, fat_type);
  
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
  setLe16(mbr->signature, MBR_SIGNATURE);

  // Temporary until exfat is setup...
  if (fat_type == FAT_TYPE_EXFAT) {
    Serial.println("TODO createPartition on ExFat");
    return;
  } else {
    // Fat16/32
    m_dev->writeSector(0, sectorBuffer);
    createFatPartition(m_dev, fat_type, 2048, sectorCount, sectorBuffer, &Serial);
  }
  
	m_dev->syncDevice();
    writeMsg("Format Done\r\n");
 
}

//----------------------------------------------------------------

void PFsLib::dump_hexbytes(const void *ptr, int len)
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
