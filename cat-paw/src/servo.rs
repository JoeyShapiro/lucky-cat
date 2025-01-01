use embedded_hal::pwm::SetDutyCycle;
use rp_pico::hal;

pub struct Servo<'a> {
    channel: &'a mut hal::pwm::Channel<
        hal::pwm::Slice<hal::pwm::Pwm0, hal::pwm::FreeRunning>,
        hal::pwm::A,
    >,
    real_max: u16,
    real_min: u16,
}

impl<'a> Servo<'a> {
    pub fn new(
        channel: &'a mut hal::pwm::Channel<
            hal::pwm::Slice<hal::pwm::Pwm0, hal::pwm::FreeRunning>,
            hal::pwm::A,
        >,
    ) -> Self {
        let real_max = (channel.max_duty_cycle() as f32 / 8.3) as u16; // ~2.4ms
        let real_min = (channel.max_duty_cycle() as f32 / 45.0) as u16; // ~0.444ms

        Self {
            channel,
            real_max,
            real_min,
        }
    }

    pub fn move_servo(
        &mut self,
        delay: &mut cortex_m::delay::Delay,
        start: u8,
        end: u8,
        velocity: u8,
    ) {
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
            let duty = (self.real_max - self.real_min) as f32 * i as f32 / u8::MAX as f32
                + self.real_min as f32;

            // cant do u16 because it would be too slow
            self.channel.set_duty_cycle(duty as u16).unwrap();
            delay.delay_ms((2.5 / speed) as u32);

            i += step;
        }
    }
}
