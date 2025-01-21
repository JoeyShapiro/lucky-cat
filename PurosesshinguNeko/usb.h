#pragma once

#include <vector>
#include <string>

struct USBDeviceInfo {
    std::string hardwareId;
    USHORT vendorId;
    USHORT productId;
    std::string description;
};

class USBDevice {
private:
    HANDLE deviceHandle;
    bool connected;

public:
    USBDevice() : deviceHandle(INVALID_HANDLE_VALUE) {
        this->connected = false;
    }
    ~USBDevice() {
        if (deviceHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(deviceHandle);
        }
    }

    static std::vector<USBDeviceInfo> ListDevices();
    bool Connect(USHORT vendorId, USHORT productId);
    bool Connected() const;
    bool SendData(const void* data, DWORD dataSize);
    bool ReceiveData(void* buffer, DWORD bufferSize, DWORD* bytesRead);
};
