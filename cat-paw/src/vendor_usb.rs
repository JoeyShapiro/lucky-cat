use usb_device::class_prelude::*;

pub struct VendorUSB<'a, B: UsbBus> {
    interface: InterfaceNumber,
    in_ep: EndpointIn<'a, B>,
    out_ep: EndpointOut<'a, B>,
}

impl<B: UsbBus> VendorUSB<'_, B> {
    /// Creates a new VendorUSB
    pub fn new(alloc: &UsbBusAllocator<B>) -> VendorUSB<'_, B> {
        VendorUSB {
            interface: alloc.interface(),
            in_ep: alloc.bulk(64),  // EP1 IN
            out_ep: alloc.bulk(64), // EP2 OUT
        }
    }

    /// Write data to host
    pub fn write(&mut self, data: &[u8]) -> Result<usize, UsbError> {
        self.in_ep.write(data)
    }

    /// Read data from host
    pub fn read(&mut self, data: &mut [u8]) -> Result<usize, UsbError> {
        self.out_ep.read(data)
    }
}

impl<B: UsbBus> UsbClass<B> for VendorUSB<'_, B> {
    fn get_configuration_descriptors(&self, writer: &mut DescriptorWriter) -> Result<(), UsbError> {
        writer.interface(self.interface, 0xFF, 0x00, 0x00)?;
        writer.endpoint(&self.in_ep)?;
        writer.endpoint(&self.out_ep)?;
        Ok(())
    }
}
