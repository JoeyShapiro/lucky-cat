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
#define VID_APPLE 0x05ac
#define PID_IPOD_CLASSIC 0x1261

int main()
{
    USBDevice device;
    if (!device.Connect(VID_APPLE, PID_IPOD_CLASSIC)) {
        device = nullptr;
    }

    PDH_HQUERY query;
    PDH_HCOUNTER cpucounter;
    PdhOpenQuery(NULL, NULL, &query);

    // this is similar to that mac thing where it asks the script thing for data
    PdhAddEnglishCounterA(query, "\\Processor Information(_Total)\\% Processor Utility", NULL, &cpucounter);
    PdhCollectQueryData(query);

    PDH_FMT_COUNTERVALUE val;

    void* data = malloc(2);
    void* buf = malloc(1);
    DWORD n = 0;

    while (device) {
        PdhCollectQueryData(query);
        PdhGetFormattedCounterValue(cpucounter, PDH_FMT_DOUBLE, NULL, &val);
        std::cout << "cpu: " << val.doubleValue << "%" << std::endl;
        
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = totalPhysMem - memInfo.ullAvailPhys;

        float mem = (float)physMemUsed / totalPhysMem;

        // normalize to byte range and send data to device
        ((u8*)data)[0] = val.doubleValue * u8_max;
        ((u8*)data)[1] = mem * u8_max;

        device.SendData(data, 2);
        device.ReceiveData(buf, 1, &n);

        Sleep(1000);
    }
}
