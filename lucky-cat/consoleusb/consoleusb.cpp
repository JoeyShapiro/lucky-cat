// consoleusb.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <string>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <string>
#include <vector>
#include <iostream>
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

struct USBDeviceInfo {
    std::string hardwareId;
    USHORT vendorId;
    USHORT productId;
    std::string description;
};

class USBDevice {
private:
    HANDLE deviceHandle;

public:
    USBDevice() : deviceHandle(INVALID_HANDLE_VALUE) {}

    static std::vector<USBDeviceInfo> ListDevices() {
        std::vector<USBDeviceInfo> deviceList;  // Changed from 'devices'

        HDEVINFO hDevInfo = SetupDiGetClassDevs(nullptr, L"USB", nullptr,
            DIGCF_ALLCLASSES | DIGCF_PRESENT);

        if (hDevInfo == INVALID_HANDLE_VALUE) {
            return deviceList;
        }

        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Enumerate through all devices
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); i++) {
            DWORD regType;
            CHAR buffer[256];
            DWORD bufferSize = sizeof(buffer);

            USBDeviceInfo currentDevice;  // Changed variable name

            // Get hardware ID
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfoData,  // Changed from deviceInfo to hDevInfo
                SPDRP_HARDWAREID, &regType, (PBYTE)buffer, bufferSize, nullptr)) {

                currentDevice.hardwareId = buffer;

                // Parse VID and PID from hardware ID
                size_t vidPos = currentDevice.hardwareId.find("VID_");
                size_t pidPos = currentDevice.hardwareId.find("PID_");

                if (vidPos != std::string::npos && pidPos != std::string::npos) {
                    try {
                        std::string vidStr = currentDevice.hardwareId.substr(vidPos + 4, 4);
                        std::string pidStr = currentDevice.hardwareId.substr(pidPos + 4, 4);

                        currentDevice.vendorId = static_cast<USHORT>(std::stoi(vidStr, nullptr, 16));
                        currentDevice.productId = static_cast<USHORT>(std::stoi(pidStr, nullptr, 16));
                    }
                    catch (...) {
                        continue; // Skip if parsing fails
                    }
                }
            }

            // Get device description
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfoData,  // Changed from deviceInfo to hDevInfo
                SPDRP_DEVICEDESC, &regType, (PBYTE)buffer, bufferSize, nullptr)) {
                currentDevice.description = buffer;
            }

            deviceList.push_back(currentDevice);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
        return deviceList;
    }

    bool Connect(USHORT vendorId, USHORT productId) {
        // Get device information set for all USB devices
        HDEVINFO deviceInfo = SetupDiGetClassDevs(nullptr, L"USB", nullptr,
            DIGCF_ALLCLASSES | DIGCF_PRESENT);

        if (deviceInfo == INVALID_HANDLE_VALUE) {
            return false;
        }

        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Enumerate through all devices
        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfo, i, &deviceInfoData); i++) {
            DWORD regType;
            CHAR buffer[256];
            DWORD bufferSize = sizeof(buffer);

            // Get device path
            if (SetupDiGetDeviceRegistryProperty(deviceInfo, &deviceInfoData,
                SPDRP_HARDWAREID, &regType, (PBYTE)buffer, bufferSize, nullptr)) {

                std::string hardwareId = buffer;
                // Check if this is our device (you'll need to modify this check)
                if (hardwareId.find("VID_" + std::to_string(vendorId)) != std::string::npos &&
                    hardwareId.find("PID_" + std::to_string(productId)) != std::string::npos) {

                    // Get device path for CreateFile
                    SP_DEVICE_INTERFACE_DATA interfaceData;
                    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

                    if (SetupDiCreateDeviceInterface(deviceInfo, &deviceInfoData,
                        nullptr, nullptr, 0, &interfaceData)) {

                        // Get required buffer size
                        DWORD requiredSize;
                        SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData,
                            nullptr, 0, &requiredSize, nullptr);

                        // Allocate buffer for device interface detail
                        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
                            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
                        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                        // Get device interface detail
                        if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData,
                            detailData, requiredSize, nullptr, nullptr)) {

                            // Open device
                            deviceHandle = CreateFile(detailData->DevicePath,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED,
                                nullptr);

                            free(detailData);
                            SetupDiDestroyDeviceInfoList(deviceInfo);
                            return deviceHandle != INVALID_HANDLE_VALUE;
                        }
                        free(detailData);
                    }
                }
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfo);
        return false;
    }

    bool SendData(const void* data, DWORD dataSize) {
        if (deviceHandle == INVALID_HANDLE_VALUE) {
            return false;
        }

        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        DWORD bytesWritten;
        bool success = WriteFile(deviceHandle, data, dataSize, &bytesWritten, &overlapped);

        if (!success && GetLastError() == ERROR_IO_PENDING) {
            // Wait for operation to complete
            success = GetOverlappedResult(deviceHandle, &overlapped, &bytesWritten, TRUE);
        }

        CloseHandle(overlapped.hEvent);
        return success && (bytesWritten == dataSize);
    }

    bool ReceiveData(void* buffer, DWORD bufferSize, DWORD* bytesRead) {
        if (deviceHandle == INVALID_HANDLE_VALUE) {
            return false;
        }

        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        bool success = ReadFile(deviceHandle, buffer, bufferSize, bytesRead, &overlapped);

        if (!success && GetLastError() == ERROR_IO_PENDING) {
            // Wait for operation to complete
            success = GetOverlappedResult(deviceHandle, &overlapped, bytesRead, TRUE);
        }

        CloseHandle(overlapped.hEvent);
        return success;
    }

    ~USBDevice() {
        if (deviceHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(deviceHandle);
        }
    }
};

#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <chrono>
#include <thread>

#include <cstdint>

uint64_t FromFileTime(const FILETIME& ft) {
    ULARGE_INTEGER uli = { 0 };
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

class SystemMonitor {
private:
    HANDLE currentProcess;
    bool initialized = false;
    FILETIME prevIdle, prevKernel, prevUser;

    ULONGLONG fileTimeToULL(const FILETIME& ft) {
        ULONGLONG ull = ft.dwHighDateTime;
        ull = (ull << 32) | ft.dwLowDateTime;
        return ull;
    }

    double calculateCPUUsage() {
        static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
        static int numProcessors;
        static bool initialized = false;

        if (!initialized) {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            numProcessors = sysInfo.dwNumberOfProcessors;

            GetSystemTimeAsFileTime((FILETIME*)&lastCPU);
            GetProcessTimes(currentProcess, (FILETIME*)&lastCPU, (FILETIME*)&lastCPU,
                (FILETIME*)&lastSysCPU, (FILETIME*)&lastUserCPU);
            initialized = true;
            return 0.0;
        }

        FILETIME nowFT, createTime, exitTime, kernelTime, userTime;
        GetSystemTimeAsFileTime(&nowFT);
        GetProcessTimes(currentProcess, &createTime, &exitTime, &kernelTime, &userTime);
        /*printf("nowft: %lu - %lu\n", nowFT.dwLowDateTime, nowFT.dwHighDateTime);
        printf("kernel: %lu - %lu\n", kernelTime.dwLowDateTime, kernelTime.dwHighDateTime);
        printf("user: %lu - %lu\n", userTime.dwLowDateTime, userTime.dwHighDateTime);*/
        //printf("%n\n", GetLastError());


        ULARGE_INTEGER now, sys, user;
        now.LowPart = nowFT.dwLowDateTime;
        now.HighPart = nowFT.dwHighDateTime;
        sys.LowPart = kernelTime.dwLowDateTime;
        sys.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

        printf("now: %llu\n", now.QuadPart);
        printf("sys: %llu\n", sys.QuadPart);
        printf("user: %llu\n", user.QuadPart);

        double percent = (sys.QuadPart - lastSysCPU.QuadPart) +
            (user.QuadPart - lastUserCPU.QuadPart);
        printf("percent: %llu - %llu) + (%llu - %llu)\n", sys.QuadPart, lastSysCPU.QuadPart, user.QuadPart, lastUserCPU.QuadPart);
        percent /= (now.QuadPart - lastCPU.QuadPart);
        percent /= numProcessors;
        percent *= 100;

        uint64_t idl = now.QuadPart - lastCPU.QuadPart;
        uint64_t ker = sys.QuadPart - lastSysCPU.QuadPart;
        uint64_t usr = user.QuadPart - lastUserCPU.QuadPart;

        float usage = 1 - (idl / (static_cast<float>(ker) + usr));
        printf("%f\n", usage);

        lastCPU = now;
        lastUserCPU = user;
        lastSysCPU = sys;

        return percent;
    }

    void getMemoryInfo(double& memoryUsagePercent, double& memoryUsedGB) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = totalPhysMem - memInfo.ullAvailPhys;

        memoryUsagePercent = (double)physMemUsed / totalPhysMem * 100.0;
        memoryUsedGB = (double)physMemUsed / (1024 * 1024 * 1024);
    }

public:
    SystemMonitor() {
        currentProcess = 0;//GetCurrentProcess();
    }

    void monitor() {
        double cpuUsage = calculateCPUUsage();
        double memoryPercent, memoryUsedGB;
        getMemoryInfo(memoryPercent, memoryUsedGB);

        std::cout << "CPU Usage: " << std::fixed << std::setprecision(1)
            << cpuUsage << "%" << std::endl;
        std::cout << "Memory Usage: " << std::fixed << std::setprecision(1)
            << memoryPercent << "% (" << memoryUsedGB << " GB)" << std::endl;
    }
};

int now() {
    FILETIME a0, a1, a2, b0, b1, b2;

    GetSystemTimes(&a0, &a1, &a2);
    SleepEx(1000, false);
    GetSystemTimes(&b0, &b1, &b2);

    uint64_t idle0 = FromFileTime(a0);
    uint64_t idle1 = FromFileTime(b0);
    uint64_t kernel0 = FromFileTime(a1);
    uint64_t kernel1 = FromFileTime(b1);
    uint64_t user0 = FromFileTime(a2);
    uint64_t user1 = FromFileTime(b2);

    uint64_t idl = idle1 - idle0;
    uint64_t ker = kernel1 - kernel0;
    uint64_t usr = user1 - user0;
    printf("idle %llu - %llu = %llu\n", idle1, idle0, idl);
    printf("kernel %llu - %llu = %llu\n", kernel1, kernel0, ker);
    printf("user %llu - %llu = %llu\n", user1, user0, usr);

    uint64_t tot = ker + usr;
    float usage = (tot - idl);
    printf("usage: %f %f\n", usage, usage/tot*100.0f);
    uint64_t cpu = (ker + usr) * 100 / (ker + usr + idl);
    printf("mine: %llu / %llu\n", ((kernel0) + user0), (kernel0 + user0 + idle0));

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numProcessors = sysInfo.dwNumberOfProcessors;

    usage = ((kernel0)+user0) / (static_cast<float>(kernel0) + user0 + idle0);
    printf("n: %d\n", numProcessors);
    printf("mine: %f\n", usage * 100);

    // CPU Usage = (1.0 - (IdleTimeDiff / (KernelTimeDiff + UserTimeDiff))) × 100
    usage = (1-(idl / (static_cast<float>(ker) + usr)));
    printf("n: %d\n", numProcessors);
    // TODO need cur and max freq. getting this info is hard
    printf("mine: %f\n", usage * 1000);

    ULONG64 cycleTime = 0;

    // Query the process cycle time
    if (!QueryProcessCycleTime(GetCurrentProcess(), &cycleTime)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to query process cycle time. Error code: " << error << std::endl;
        return 1;
    }

    printf("cycles: %lu\n", cycleTime);

    return static_cast<int>(cpu);
}

int main() {
    printf("hello\n");
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

    // Now you can connect to a specific device if you want
    USBDevice device;
    if (device.Connect(0x0483, 0x5740)) {  // Replace with your VID/PID
        std::cout << "Successfully connected to device!" << std::endl;
    }

    SystemMonitor monitor;

    while (true) {
        printf("now: %d\n\n\n", now());
        //monitor.monitor();
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
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
