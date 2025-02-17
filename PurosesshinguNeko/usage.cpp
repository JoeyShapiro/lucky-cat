#include "usage.h"

#pragma comment(lib, "pdh.lib")

Usage::Usage() {
    this->cpu = 0;
    this->memory = 0;

    PdhOpenQuery(NULL, NULL, &this->query);

    // this is similar to that mac thing where it asks the script thing for data
    // processor Utility goes over 100%
    PdhAddEnglishCounterA(this->query, "\\Processor Information(_Total)\\% Processor Time", NULL, &this->cpuCounter);
    PdhCollectQueryData(this->query);

    PDH_FMT_COUNTERVALUE val;
    PdhCollectQueryData(this->query);
    PdhGetFormattedCounterValue(this->cpuCounter, PDH_FMT_DOUBLE, NULL, &val);
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
    this->cpu = val.doubleValue / 100;

    return true;
}

float Usage::CPU() const {
    return this->cpu;
}

float Usage::Mem() const {
    return this->memory;
}
