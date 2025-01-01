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

fn move_servo(
    channel: &mut hal::pwm::Channel<
        hal::pwm::Slice<hal::pwm::Pwm0, hal::pwm::FreeRunning>,
        hal::pwm::A,
    >,
    delay: &mut cortex_m::delay::Delay,
    start: u8,
    end: u8,
    velocity: u8,
) {
    let real_max = (channel.max_duty_cycle() as f32 / 8.3) as u16; // ~2.4ms
    let real_min = (channel.max_duty_cycle() as f32 / 45.0) as u16; // ~0.444ms

    // convert to range of 0 - 1
    let speed = velocity as f32 / u8::MAX as f32;

    // must be bigger than start and end
    let step = if end > start { 1 } else { -1 };

    // cant use for with rev
    let mut i = start as i16;
    loop {
        // break if we are at the end
        if step == 1 && i >= end as i16 {
            break;
        } else if step == -1 && i <= end as i16 {
            break;
        }

        // convert i to a duty cycle
        let duty = (real_max - real_min) as f32 * i as f32 / u8::MAX as f32 + real_min as f32;

        // cant do u16 because it would be too slow
        channel.set_duty_cycle(duty as u16).unwrap();
        delay.delay_ms((2.5 / speed) as u32);

        i += step;
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
    pwm.set_ph_correct();
    pwm.set_div_int(20u8); // 50 hz
    pwm.enable();

    // Output channel B on PWM4 to the LED pin
    let channel = &mut pwm.channel_a;
    // channel.output_to(pins.led);
    channel.output_to(pins.gpio0);
    let max = channel.max_duty_cycle();
    // find better way
    // finally got it. servo doesnt use whole duty cycle, just a portion
    // so max is 20ms, but we only use 1-2ms
    // so it was jumping around because it was trying to use the whole duty cycle
    // its actually 0.5-2.5ms
    // TODO make this more readable, maybe decimals or something
    let _real_max = (max as f32 / 8.3) as u16; // ~2.4ms
    let real_min = (max as f32 / 45f32) as u16; // ~0.444ms

    let mut pin_led = pins.led.into_push_pull_output();

    // blink twice
    for _ in 0..2 {
        pin_led.set_high().unwrap();
        delay.delay_ms(100);
        pin_led.set_low().unwrap();
        delay.delay_ms(100);
    }

    // reset
    channel.set_duty_cycle(real_min).unwrap();
    delay.delay_ms(1000);

    loop {
        move_servo(channel, &mut delay, 0, u8::MAX, velocity);
        move_servo(channel, &mut delay, u8::MAX, 0, velocity);

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
