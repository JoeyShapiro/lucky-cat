#pragma once

#include <vector>

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
    ~USBDevice() {
        if (deviceHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(deviceHandle);
        }
    }

    static std::vector<USBDeviceInfo> ListDevices();
    bool Connect(USHORT vendorId, USHORT productId);
    bool SendData(const void* data, DWORD dataSize);
    bool ReceiveData(void* buffer, DWORD bufferSize, DWORD* bytesRead);
};
