import usb.core
import usb.util
import sys
import usb.backend.libusb1 as libusb1

class USBDevice:
    @staticmethod
    def list_all_devices():
        """
        List all USB devices connected to the system
        
        Returns:
            list: List of dictionaries containing device information
        """
        devices = []
        for device in usb.core.find(find_all=True):
            try:
                device_info = {
                    'vendor_id': hex(device.idVendor),
                    'product_id': hex(device.idProduct),
                    'manufacturer': usb.util.get_string(device, device.iManufacturer) if device.iManufacturer else 'Unknown',
                    'product': usb.util.get_string(device, device.iProduct) if device.iProduct else 'Unknown',
                    'serial_number': usb.util.get_string(device, device.iSerialNumber) if device.iSerialNumber else 'Unknown',
                    'bus': device.bus,
                    'address': device.address
                }
                devices.append(device_info)
            except:
                # Some devices might not be accessible or might raise errors
                devices.append({
                    'vendor_id': hex(device.idVendor),
                    'product_id': hex(device.idProduct),
                    'manufacturer': 'Access Denied',
                    'product': 'Access Denied',
                    'serial_number': 'Access Denied',
                    'bus': device.bus,
                    'address': device.address
                })
        return devices

    def __init__(self, vendor_id, product_id):
        """
        Initialize USB device communication
        
        Args:
            vendor_id (hex): Vendor ID of the USB device
            product_id (hex): Product ID of the USB device
        """
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.device = None
        self.endpoint_in = None
        self.endpoint_out = None
    
    def connect(self):
        """Connect to the USB device and configure endpoints"""
        # Find the device
        self.device = usb.core.find(idVendor=self.vendor_id, idProduct=self.product_id)
        
        if self.device is None:
            raise ValueError('Device not found')
            
        # Set configuration
        self.device.set_configuration()
        
        # Get configuration
        cfg = self.device.get_active_configuration()
        
        # Get interface
        interface_number = cfg[(0,0)].bInterfaceNumber
        intf = usb.util.find_descriptor(
            cfg, bInterfaceNumber=interface_number
        )
        
        # Find endpoints
        self.endpoint_out = usb.util.find_descriptor(
            intf,
            custom_match=lambda e: 
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
        )
        
        self.endpoint_in = usb.util.find_descriptor(
            intf,
            custom_match=lambda e: 
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
        )
        
        if self.endpoint_out is None or self.endpoint_in is None:
            raise ValueError('Endpoints not found')
    
    def write(self, data, timeout=1000):
        """
        Write data to the USB device
        
        Args:
            data (bytes): Data to send to the device
            timeout (int): Timeout in milliseconds
        
        Returns:
            int: Number of bytes written
        """
        try:
            return self.endpoint_out.write(data, timeout=timeout)
        except usb.core.USBError as e:
            print(f"Write error: {str(e)}")
            return 0
    
    def read(self, size=64, timeout=1000):
        """
        Read data from the USB device
        
        Args:
            size (int): Number of bytes to read
            timeout (int): Timeout in milliseconds
        
        Returns:
            bytes: Data read from the device
        """
        try:
            return self.endpoint_in.read(size, timeout=timeout)
        except usb.core.USBError as e:
            print(f"Read error: {str(e)}")
            return None
    
    def close(self):
        """Release the USB device"""
        if self.device:
            usb.util.dispose_resources(self.device)

# Example usage
def main():
    # Replace with your device's vendor ID and product ID
    VENDOR_ID = 0x05ac  # Example vendor ID
    PRODUCT_ID = 0x1261  # Example product ID
    
    print(USBDevice.list_all_devices())
    
    try:
        # Create device instance
        device = USBDevice(VENDOR_ID, PRODUCT_ID)
        
        # Connect to device
        device.connect()
        
        # Example: Write data
        data_to_send = b"Hello USB Device"
        bytes_written = device.write(data_to_send)
        print(f"Wrote {bytes_written} bytes")
        
        # Example: Read response
        response = device.read()
        if response:
            print(f"Received: {response}")
        
    except Exception as e:
        print(f"Error: {str(e)}")
    
    finally:
        device.close()

if __name__ == "__main__":
    main()
