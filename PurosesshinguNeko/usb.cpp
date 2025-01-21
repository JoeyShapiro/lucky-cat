#include <iostream>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <string>
#include <vector>
#include <psapi.h>
#include <chrono>
#include <thread>
#include <iomanip>

#include "usb.h"

#pragma comment(lib, "SetupAPI.lib")

std::vector<USBDeviceInfo> USBDevice::ListDevices() {
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

			// utf-16 to ascii
			for (size_t i = 0; i < bufferSize; i+=2)
			{
				currentDevice.description.append(1, buffer[i]);
				if (buffer[i] == '\0') break;
			}
		}

		deviceList.push_back(currentDevice);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return deviceList;
}

bool USBDevice::Connected() const {
	return this->connected;
}

bool USBDevice::Connect(USHORT vendorId, USHORT productId) {
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

			// convert utf16 to utf8
			for (size_t j = 0; j < bufferSize; j+=2)
			{
				if (buffer[j]) {
					hardwareId += buffer[j];
				}
				else {
					break;
				}

			}
			hardwareId += '\0';

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
						this->connected = deviceHandle != INVALID_HANDLE_VALUE;
						return this->connected;
					}
					free(detailData);
				}
			}
		}
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);
	return false;
}

bool USBDevice::SendData(const void* data, DWORD dataSize) {
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

bool USBDevice::ReceiveData(void* buffer, DWORD bufferSize, DWORD* bytesRead) {
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