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
#include "PFsLib.h"
PFsVolume* PFsVolume::m_cwv = nullptr;
//------------------------------------------------------------------------------
bool PFsVolume::begin(USBMSCDevice* dev, bool setCwv, uint8_t part) {
  m_usmsci = dev;
  Serial.printf("PFsVolume::begin USBmscInterface(%x, %u)\n", (uint32_t)dev, part);  
  return begin((BlockDevice*)dev, setCwv, part);
}

bool PFsVolume::begin(BlockDevice* blockDev, bool setCwv, uint8_t part) {
  Serial.printf("PFsVolume::begin(%x, %u)\n", (uint32_t)blockDev, part);
  m_blockDev = blockDev;
  m_part = part;
  m_fVol = nullptr;
  m_xVol = new (m_volMem) ExFatVolume;
  if (m_xVol && m_xVol->begin(m_blockDev, setCwv, part)) {
    goto done;
  }
  m_xVol = nullptr;
  m_fVol = new (m_volMem) FatVolume;
  if (m_fVol && m_fVol->begin(m_blockDev, setCwv, part)) {
    goto done;
  }
  m_cwv = nullptr;
  m_fVol = nullptr;
  return false;

 done:
  m_cwv = this;
  return true;
}
//------------------------------------------------------------------------------
bool PFsVolume::ls(print_t* pr, const char* path, uint8_t flags) {
  PFsBaseFile dir;
  return dir.open(this, path, O_RDONLY) && dir.ls(pr, flags);
}
//------------------------------------------------------------------------------
PFsFile PFsVolume::open(const char *path, oflag_t oflag) {
  PFsFile tmpFile;
  tmpFile.open(this, path, oflag);
  return tmpFile;
}
bool PFsVolume::getVolumeLabel(char *volume_label, size_t cb) 
{
  char buf[32];
  if (!volume_label || (cb < 12)) return false; // don't want to deal with it

  PFsFile root;
  if (!root.openRoot(this)) return false;
  root.read(buf, 32);

  switch (fatType())
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
          volume_label[i]  = dir->name[i];
        }
        while ((i > 0) && (volume_label[i - 1] == ' ')) i--; // trim off trailing blanks
        volume_label[i] = 0;
      }
      break;
    case FAT_TYPE_EXFAT:
      {
        DirLabel_t *dir;
        dir = reinterpret_cast<DirLabel_t*>(buf);
        if (dir->type != EXFAT_TYPE_LABEL) return false; // not a label?
        size_t i;
        for (i = 0; i < dir->labelLength; i++) {
          volume_label[i] = dir->unicode[2 * i];
        }
        volume_label[i] = 0;
      }
      break;
  }
  return true;

}



typedef struct {
  uint32_t free;
  uint32_t todo;
  uint32_t clusters_per_sector;
} _gfcc_t;


static void _getfreeclustercountCB(uint32_t token, uint8_t *buffer) 
{
  //digitalWriteFast(1, HIGH);
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

  //digitalWriteFast(1, LOW);
}

//-------------------------------------------------------------------------------------------------
uint32_t PFsVolume::freeClusterCount()  {
  // For XVolume lets let the original code do it.
  if (m_xVol) return m_xVol->freeClusterCount();

  if (!m_fVol) return 0;

  if (!m_usmsci) return m_fVol->freeClusterCount();

  // So roll our own here for Fat16/32...
  _gfcc_t gfcc; 
  gfcc.free = 0;

  switch (m_fVol->fatType()) {
    default: return 0;
    case FAT_TYPE_FAT16: gfcc.clusters_per_sector = 512/2; break;
    case FAT_TYPE_FAT32: gfcc.clusters_per_sector = 512/4; break;
  }
  gfcc.todo = m_fVol->clusterCount() + 2;

//  digitalWriteFast(0, HIGH);
  m_usmsci->readSectorsWithCB(m_fVol->fatStartSector(), gfcc.todo / gfcc.clusters_per_sector + 1, 
      &_getfreeclustercountCB, (uint32_t)&gfcc);
//  digitalWriteFast(0, LOW);

  return gfcc.free;
}

uint32_t PFsVolume::getFSInfoSectorFreeClusterCount() {
  uint8_t sector_buffer[512];
  if (fatType() != FAT_TYPE_FAT32) return (uint32_t)-1;

  // We could probably avoid this read if our class remembered the starting sector number for the partition...
  if (!m_blockDev->readSector(0, sector_buffer)) return (uint32_t)-1;
  MbrSector_t *mbr = reinterpret_cast<MbrSector_t*>(sector_buffer);
  MbrPart_t *pt = &mbr->part[m_part - 1];
  BpbFat32_t* bpb;
  if ((pt->type != 11) && (pt->type != 12))  return (uint32_t)-1;

  uint32_t volumeStartSector = getLe32(pt->relativeSectors);
  if (!m_blockDev->readSector(volumeStartSector, sector_buffer)) return (uint32_t)-1;
  pbs_t *pbs = reinterpret_cast<pbs_t*> (sector_buffer);
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  
  //Serial.println("\nReadFat32InfoSectorFree BpbFat32_t sector");
  //dump_hexbytes(sector_buffer, 512);
  uint16_t infoSector = getLe16(bpb->fat32FSInfoSector); 

  // I am assuming this sector is based off of the volumeStartSector... So try reading from there.
  //Serial.printf("Try to read Info sector (%u)\n", infoSector); Serial.flush(); 
  if (!m_blockDev->readSector(volumeStartSector+infoSector, sector_buffer)) return (uint32_t)-1;
  //dump_hexbytes(sector_buffer, 512);
  FsInfo_t *pfsi = reinterpret_cast<FsInfo_t*>(sector_buffer);

  // check signatures:
  if (memcmp(pfsi->leadSignature, "RRaA", 4) != 0) Serial.println("Lead Sig wrong");    
  if (memcmp(pfsi->structSignature, "rrAa", 4) != 0) Serial.println("struct Sig wrong");    
  static const uint8_t _trail_sig[4] = {0x00, 0x00, 0x55, 0xAA};
  if (memcmp(pfsi->trailSignature, _trail_sig, 4) != 0) Serial.println("Trail Sig wrong");    
  uint32_t free_count = getLe32(pfsi->freeCount);
  return free_count;


}

bool PFsVolume::setUpdateFSInfoSectorFreeClusterCount(uint32_t free_count) {
  uint8_t sector_buffer[512];
  if (fatType() != FAT_TYPE_FAT32) return (uint32_t)false;

  if (free_count == (uint32_t)-1) free_count = freeClusterCount();

  // We could probably avoid this read if our class remembered the starting sector number for the partition...
  if (!m_blockDev->readSector(0, sector_buffer)) return (uint32_t)-1;
  MbrSector_t *mbr = reinterpret_cast<MbrSector_t*>(sector_buffer);
  MbrPart_t *pt = &mbr->part[m_part - 1];
  BpbFat32_t* bpb;
  if ((pt->type != 11) && (pt->type != 12))  return (uint32_t)-1;

  uint32_t volumeStartSector = getLe32(pt->relativeSectors);
  if (!m_blockDev->readSector(volumeStartSector, sector_buffer)) return (uint32_t)-1;
  pbs_t *pbs = reinterpret_cast<pbs_t*> (sector_buffer);
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  
  //Serial.println("\nReadFat32InfoSectorFree BpbFat32_t sector");
  //dump_hexbytes(sector_buffer, 512);
  uint16_t infoSector = getLe16(bpb->fat32FSInfoSector); 

  // OK we now need to fill in the the sector with the appropriate information...
  // Not sure if we should read it first or just blast out new data...
  FsInfo_t *pfsi = reinterpret_cast<FsInfo_t*>(sector_buffer);
  memset(sector_buffer, 0 , 512);

  // write FSINFO sector and backup
  setLe32(pfsi->leadSignature, FSINFO_LEAD_SIGNATURE);
  setLe32(pfsi->structSignature, FSINFO_STRUCT_SIGNATURE);
  setLe32(pfsi->freeCount, free_count);
  setLe32(pfsi->nextFree, 0XFFFFFFFF);
  if (!m_blockDev->writeSector(volumeStartSector+infoSector, sector_buffer)) return (uint32_t) false;
  return true;
}



#if ENABLE_ARDUINO_STRING
//------------------------------------------------------------------------------
PFsFile PFsVolume::open(const String &path, oflag_t oflag) {
  return open(path.c_str(), oflag );
}
#endif  // ENABLE_ARDUINO_STRING
