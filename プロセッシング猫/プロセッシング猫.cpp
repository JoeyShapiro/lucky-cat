// プロセッシング猫.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

/*
プロ (puro) = "pro"
セッ (ses) = "ces"
シン (shin) = "s-in"
グ (gu) = "g"
猫 (neko) = "cat"
*/

#include <iostream>
#include <pdh.h>
#include "usb.h"

#pragma comment(lib, "pdh.lib")

/*
I send over the percentage of usage to the device normalized to its range of 1 byte
it will convert that to whatever it needs
in this case it will be used to determine how far it should move from the origin
*/
#define u8 unsigned char
#define u8_max sizeof(u8) * 255


int main()
{
    std::cout << "Hello World!\n";
    printf("max: %d\n", u8_max);

    // Get list of all USB devices
    auto devices = USBDevice::ListDevices();

    // Print information about each device
    for (const auto& device : devices) {
        std::cout << "Description: " << device.description << std::endl;
        std::cout << "Vendor ID: 0x" << std::hex << device.vendorId << std::endl;
        std::cout << "Product ID: 0x" << std::hex << device.productId << std::endl;
        std::cout << "Hardware ID: " << device.hardwareId << std::endl;
        std::cout << "------------------------" << std::endl;
    }

    PDH_HQUERY query;
    PDH_HCOUNTER cpucounter;
    PDH_HCOUNTER memcounter;
    PdhOpenQuery(NULL, NULL, &query);

    // this is similar to that mac thing where it asks the script thing for data
    PdhAddEnglishCounterA(query, "\\Processor Information(_Total)\\% Processor Utility", NULL, &cpucounter);
    PdhAddEnglishCounterA(query, "\\Memory\\% Committed Bytes in Use", NULL, &memcounter); // "\\Memory\\Available Bytes"
    PdhCollectQueryData(query);

    PDH_FMT_COUNTERVALUE val;

    while (1) {
        PdhCollectQueryData(query);
        PdhGetFormattedCounterValue(cpucounter, PDH_FMT_DOUBLE, NULL, &val);
        std::cout << "cpu: " << val.doubleValue << "%" << std::endl;
        
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = totalPhysMem - memInfo.ullAvailPhys;

        float mem = (float)physMemUsed / totalPhysMem;

        // normalize to byte range
        u8 norm = mem * u8_max;
        printf("mem: %d\n", norm);

        Sleep(1000);
    }
}
