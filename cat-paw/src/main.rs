#![no_std]
#![no_main]

mod servo;
mod vendor_usb;

use embedded_hal::digital::OutputPin;
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

const VID_APPLE: u16 = 0x05ac;
const PID_IPOD_CLASSIC: u16 = 0x1261;

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

    // Set up the USB driver
    let usb_bus = UsbBusAllocator::new(hal::usb::UsbBus::new(
        pac.USBCTRL_REGS,
        pac.USBCTRL_DPRAM,
        clocks.usb_clock,
        true,
        &mut pac.RESETS,
    ));

    // Set up the USB Communications Class Device driver
    let mut serial = vendor_usb::VendorUSB::new(&usb_bus);

    let mut amplitude = 0; // cant be greater than 127
    let mut velocity = 0;

    // Create a USB device with a fake VID and PID
    let mut usb_dev = UsbDeviceBuilder::new(&usb_bus, UsbVidPid(VID_APPLE, PID_IPOD_CLASSIC))
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

    let mut pin_led = pins.led.into_push_pull_output();

    let mut paw = servo::Servo::new(channel);

    // blink twice
    for _ in 0..2 {
        pin_led.set_high().unwrap();
        delay.delay_ms(100);
        pin_led.set_low().unwrap();
        delay.delay_ms(100);
    }

    loop {
        paw.update(&mut delay);

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

                    // safety check to remove the last one
                    amplitude &= 0x7F;
                    paw.set_amp(amplitude);

                    velocity = buf[1] as u8;
                    paw.set_speed(velocity);

                    // pin_led.set_high().unwrap();
                    // delay.delay_ms(1);
                    // pin_led.set_low().unwrap();

                    // send back 0x01 if we got the right data
                    let _ = serial.write(&[0x01]);
                }
            }
        }
    }
}
