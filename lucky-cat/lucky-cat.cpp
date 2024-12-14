// lucky-cat.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <pdh.h>

#pragma comment(lib, "pdh.lib")


int main()
{
    std::cout << "Hello World!\n";

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
        PdhGetFormattedCounterValue(memcounter, PDH_FMT_DOUBLE, NULL, &val);
        std::cout << "mem avail: " << val.doubleValue << std::endl;

        Sleep(1000);
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
