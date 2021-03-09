/**
 * Copyright (c) 2011-2020 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "PFsFatFormatter.h"

// Set nonzero to use calculated CHS in MBR.  Should not be required.
#define USE_LBA_TO_CHS 1

// Constants for file system structure optimized for flash.
uint16_t const BU16 = 128;
uint16_t const BU32 = 8192;
// Assume 512 byte sectors.
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTORS_PER_MB = 0X100000/BYTES_PER_SECTOR;
const uint16_t FAT16_ROOT_ENTRY_COUNT = 512;
const uint16_t FAT16_ROOT_SECTOR_COUNT =
               32*FAT16_ROOT_ENTRY_COUNT/BYTES_PER_SECTOR;
//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
#elif defined(__AVR__)
#define writeMsg(str) if (m_pr) m_pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) if (m_pr) m_pr->write(str)
#endif  // PRINT_FORMAT_PROGRESS
//------------------------------------------------------------------------------
bool PFsFatFormatter::format(BlockDeviceInterface *blockDev, uint8_t part, PFsVolume &partVol, uint8_t* secBuf, print_t* pr) {
  MbrSector_t mbr;

  bool rtn;
  m_secBuf = secBuf;
  m_pr = pr;
  m_dev = blockDev;
  
  if (!m_dev->readSector(0, (uint8_t*)&mbr)) {
    Serial.print("\nread MBR failed.\n");
    //errorPrint();
    return false;
  }
  
  MbrPart_t *pt = &mbr.part[part];
  m_sectorCount = getLe32(pt->totalSectors);
  m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;
  
  Serial.println("\nPFsFatFormatter::format................");
  Serial.printf("Sector Count: %d, Sectors/MB: %d\n", m_sectorCount, SECTORS_PER_MB);
  Serial.printf("Partition Capacity (MB): %d\n", m_capacityMB);
  
  m_dataStart = partVol.dataStartSector();
  m_sectorsPerCluster = partVol.sectorsPerCluster();
  m_relativeSectors = getLe32(pt->relativeSectors);
  
    Serial.printf("    m_dataStart:%u\n", m_dataStart);
	Serial.printf("    m_sectorsPerCluster:%u\n", m_sectorsPerCluster);
	Serial.printf("    m_relativeSectors:%u\n", m_relativeSectors);
	
	for(uint8_t i = 0; i < 3; i++) {
		begin_CHS[i] = uint8_t(pt->beginCHS[i]);
        Serial.print("0x"); Serial.print(begin_CHS[i], HEX); Serial.print( ',');
	}
	
	for(uint8_t i = 0; i < 3; i++) {
		end_CHS[i] = uint8_t(pt->endCHS[i]);
		Serial.print("0x"); Serial.print(end_CHS[i], HEX); Serial.print( ',');
	}
	Serial.println();
	
	
  
  if (m_capacityMB <= 6) {
    writeMsg("Card is too small.\r\n");
    return false;
  } else if (m_capacityMB <= 16) {
    m_sectorsPerCluster = 2;
  } else if (m_capacityMB <= 32) {
    m_sectorsPerCluster = 4;
  } else if (m_capacityMB <= 64) {
    m_sectorsPerCluster = 8;
  } else if (m_capacityMB <= 128) {
    m_sectorsPerCluster = 16;
  } else if (m_capacityMB <= 1024) {
    m_sectorsPerCluster = 32;
  } else if (m_capacityMB <= 32768) {
    m_sectorsPerCluster = 64;
  } else {
    // SDXC cards
    m_sectorsPerCluster = 128;
  }
  rtn = m_sectorCount < 0X400000 ? makeFat16() :makeFat32();
  if (rtn) {
    writeMsg("Format Done\r\n");
  } else {
    writeMsg("Format Failed\r\n");
  }
  return rtn;
}

//------------------------------------------------------------------------------
bool PFsFatFormatter::makeFat16() {
	
  Serial.printf(" MAKEFAT16\n");

  uint32_t nc;
  uint32_t r;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  /*
  for (m_dataStart = 2*BU16; ; m_dataStart += BU16) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
    r = BU16 + 1 + 2*m_fatSize + FAT16_ROOT_SECTOR_COUNT;
    if (m_dataStart >= r) {
      m_relativeSectors = m_dataStart - r + BU16;
      break;
    }
  }
*/


  nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
  // check valid cluster count for FAT16 volume
  if (nc < 4085 || nc >= 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = 1;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  //m_totalSectors = nc*m_sectorsPerCluster
  //                 + 2*m_fatSize + m_reservedSectorCount + 32;
  if (m_totalSectors < 65536) {
    m_partType = 0X04;
  } else {
    m_partType = 0X06;
  }
  
  Serial.printf("partType: %d, fatStart: %d, totalSectors: %d\n", m_partType, m_fatStart, m_totalSectors);
  // write MBR
  if (!writeMbr()) {
    return false;
  }
  
  return 1;
}

//------------------------------------------------------------------------------
bool PFsFatFormatter::makeFat32() {
	Serial.printf(" MAKEFAT32\n");
  uint32_t nc;
  uint32_t r;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  FsInfo_t* fsi = reinterpret_cast<FsInfo_t*>(m_secBuf);
/*
  m_relativeSectors = BU32;
  for (m_dataStart = 2*BU32; ; m_dataStart += BU32) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
    r = m_relativeSectors + 9 + 2*m_fatSize;
    if (m_dataStart >= r) {
      break;
    }
  }
*/
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;

  // error if too few clusters in FAT32 volume
  if (nc < 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = m_dataStart - m_relativeSectors - 2*m_fatSize;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  //m_totalSectors = nc*m_sectorsPerCluster + m_dataStart - m_relativeSectors;
  // type depends on address of end sector
  // max CHS has lba = 16450560 = 1024*255*63
  if ((m_relativeSectors + m_totalSectors) <= 16450560) {
    // FAT32 with CHS and LBA
    m_partType = 0X0B;
  } else {
    // FAT32 with only LBA
    m_partType = 0X0C;
  }
  //Write MBR
  Serial.printf("partType: %d, fatStart: %d, totalSectors: %d\n", m_partType, m_fatStart, m_totalSectors);
  if (!writeMbr()) {
    return false;
  }

  return 1;
}

//------------------------------------------------------------------------------
bool PFsFatFormatter::writeMbr() {
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);
  
  Serial.println("\nPFsFatFormatter::writeMbr.....");

#if USE_LBA_TO_CHS
Serial.println("Using USE_LBA_TO_CHS");
  lbaToMbrChs(begin_CHS, m_capacityMB, m_relativeSectors);
  lbaToMbrChs(end_CHS, m_capacityMB,
              m_relativeSectors + m_totalSectors -1);
#else  // USE_LBA_TO_CHS
  begin_CHS[0] = 1;
  begin_CHS[1] = 1;
  begin_CHS[2] = 0;
  end_CHS[0] = 0XFE;
  end_CHS[1] = 0XFF;
  end_CHS[2] = 0XFF;
#endif  // USE_LBA_TO_CHS

  mbr->part->type = m_partType;
  //setLe32(mbr->part->relativeSectors, m_relativeSectors);
  //setLe32(mbr->part->totalSectors, m_totalSectors);
  //setLe16(mbr->signature, MBR_SIGNATURE);
  //return m_dev->writeSector(0, m_secBuf);
  return 1;
}

