#include "usage.h"
#include <Pdh.h>

#pragma comment(lib, "pdh.lib")

Usage::Usage() {
    this->cpu = 0;
    this->memory = 0;

    PdhOpenQuery(NULL, NULL, this->query);

    // this is similar to that mac thing where it asks the script thing for data
    PdhAddEnglishCounterA(this->query, "\\Processor Information(_Total)\\% Processor Utility", NULL, &this->cpuCounter);
    PdhCollectQueryData(this->query);
}

bool Usage::Measure() {
    PDH_FMT_COUNTERVALUE val;
    PdhCollectQueryData(this->query);
    PdhGetFormattedCounterValue(this->cpuCounter, PDH_FMT_DOUBLE, NULL, &val);

    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = totalPhysMem - memInfo.ullAvailPhys;

    this->memory = (float)physMemUsed / totalPhysMem;
    this->cpu = val.doubleValue;

    return true;
}

float Usage::CPU() const {
    return this->cpu;
}

float Usage::Mem() const {
    return this->memory;
}
