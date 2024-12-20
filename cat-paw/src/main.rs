//! # Pico USB Serial Example
//!
//! Creates a USB Serial device on a Pico board, with the USB driver running in
//! the main thread.
//!
//! This will create a USB Serial device echoing anything it receives. Incoming
//! ASCII characters are converted to upercase, so you can tell it is working
//! and not just local-echo!
//!
//! See the `Cargo.toml` file for Copyright and license details.

#![no_std]
#![no_main]

use embedded_hal::{digital::OutputPin, pwm::SetDutyCycle};
// The macro for our start-up function
use rp_pico::{entry, hal::Clock};

// Ensure we halt the program on panic (if we don't mention this crate it won't
// be linked)
use panic_halt as _;

// A shorter alias for the Peripheral Access Crate, which provides low-level
// register access
use rp_pico::hal::pac;

// A shorter alias for the Hardware Abstraction Layer, which provides
// higher-level drivers.
use rp_pico::hal;

// USB Device support
use usb_device::{class_prelude::*, prelude::*};

// USB Communications Class Device support
use usbd_serial::SerialPort;

// Used to demonstrate writing formatted strings
use core::{fmt::Write, pin};
use heapless::String;

// The minimum PWM value (i.e. LED brightness) we want
const LOW: u16 = 0;

// The maximum PWM value (i.e. LED brightness) we want
const HIGH: u16 = 25000;

/// Custom USB class for our device
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

/// Entry point to our bare-metal application.
///
/// The `#[entry]` macro ensures the Cortex-M start-up code calls this function
/// as soon as all global variables are initialised.
///
/// The function configures the RP2040 peripherals, then echoes any characters
/// received over USB Serial.
#[entry]
fn main() -> ! {
    // Grab our singleton objects
    let mut pac = pac::Peripherals::take().unwrap();

    // Set up the watchdog driver - needed by the clock setup code
    let mut watchdog = hal::Watchdog::new(pac.WATCHDOG);

    // Configure the clocks
    //
    // The default is to generate a 125 MHz system clock
    let clocks = hal::clocks::init_clocks_and_plls(
        rp_pico::XOSC_CRYSTAL_FREQ,
        pac.XOSC,
        pac.CLOCKS,
        pac.PLL_SYS,
        pac.PLL_USB,
        &mut pac.RESETS,
        &mut watchdog,
    )
    .ok()
    .unwrap();

    let core = pac::CorePeripherals::take().unwrap();

    let timer = hal::Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);

    // Set up the USB driver
    let usb_bus = UsbBusAllocator::new(hal::usb::UsbBus::new(
        pac.USBCTRL_REGS,
        pac.USBCTRL_DPRAM,
        clocks.usb_clock,
        true,
        &mut pac.RESETS,
    ));

    // Set up the USB Communications Class Device driver
    let mut serial = VendorUSB::new(&usb_bus);

    let mut amplitude = 0;
    let mut velocity = 0;

    // Create a USB device with a fake VID and PID
    let mut usb_dev = UsbDeviceBuilder::new(&usb_bus, UsbVidPid(0x05ac, 0x1261))
        .strings(&[StringDescriptors::default()
            .manufacturer("Apple")
            .product("Cat Paw")
            .serial_number("catpaw")])
        .unwrap()
        .device_class(0xff) // 2 // from: https://www.usb.org/defined-class-codes
        .build();

    // The single-cycle I/O block controls our GPIO pins
    let sio = hal::Sio::new(pac.SIO);

    // Set the pins up according to their function on this particular board
    let pins = rp_pico::Pins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        sio.gpio_bank0,
        &mut pac.RESETS,
    );

    // The delay object lets us wait for specified amounts of time (in
    // milliseconds)
    let mut delay = cortex_m::delay::Delay::new(core.SYST, clocks.system_clock.freq().to_Hz());

    // Init PWMs
    let mut pwm_slices = hal::pwm::Slices::new(pac.PWM, &mut pac.RESETS);

    // Configure PWM0
    // could make my own pwm, but this is fine
    // div doesnt seem to do anything
    let pwm = &mut pwm_slices.pwm0;
    // pwm.set_ph_correct();
    // pwm.set_div_int(20u8); // 50 hz
    pwm.enable();

    // Output channel B on PWM4 to the LED pin
    let channel = &mut pwm.channel_a;
    // channel.output_to(pins.led);
    channel.output_to(pins.gpio0);

    let mut pin_in1 = pins.gpio1.into_push_pull_output();
    let mut pin_in2 = pins.gpio2.into_push_pull_output();
    // pint_en1.set_high().unwrap();
    // pin_in1.set_high().unwrap();
    // channel.set_duty_cycle(65535).unwrap();

    let mut pin_led = pins.led.into_push_pull_output();

    loop {
        // Check for new data
        if usb_dev.poll(&mut [&mut serial]) {
            let mut buf = [0u8; 64];
            match serial.read(&mut buf) {
                Err(_e) => {
                    // Do nothing
                }
                Ok(0) => {
                    // Do nothing
                }
                Ok(count) => {
                    if count != 2 {
                        continue; // ignore anything that isn't 2 bytes
                    }

                    // first byte is amp, then velocity
                    amplitude = buf[0] as u8;
                    velocity = buf[1] as u8;

                    pin_led.set_high().unwrap();
                    delay.delay_ms(1);
                    pin_led.set_low().unwrap();

                    // send back 0x01 if we got the right data
                    let _ = serial.write(&[0x01]);
                }
            }
        }
    }
}
