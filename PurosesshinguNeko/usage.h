#pragma once

#include <Pdh.h>

class Usage {
private:
    float cpu;
    float memory;
    PDH_HQUERY *query;
    PDH_HCOUNTER cpuCounter;

public:
    Usage();
    ~Usage() {}

    bool Measure();
    float CPU() const;
    float Mem() const;
};
