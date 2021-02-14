# mscFS Library

mscFS is a version of SD for use with USB host mass storeage devices and uses the new Teensy File System Abstracion class.

This library is compatible with SD. It is ment for use with Sdfat and USBHost_t36. It parallels SD and uses FS.h.
FS.h was not modified but SdFat-parallel had slight modifications to "SdFat.h" and "SdFatConfig.h".

Three libraries must be cloned and installed in the TD1.54B5 libraries folder.

You will need to backup these two libraries:

1 - SdFat

2 - USBHost_t36

The two libraries you need with this library are available here:

https://github.com/wwatson4506/SdFat-beta/tree/SdFat-parrallel

https://github.com/wwatson4506/USBHost_t36

The actual MSC library Here:

https://github.com/wwatson4506/MSC

is not needed now with the modified version of USBHost_t36. "MasStorageDriver.cpp" and "msc.h" are now contained in my version of USBHost_t36. "MassStorageHost.cpp" and "MassStorage.h" are still available in the four different version of the MSC library.
