use embedded_hal::pwm::SetDutyCycle;
use rp_pico::hal;

const U8_HALF: u8 = 0x7f;
const MAX_SPEED: f32 = 2.5;

// This is more the paw, but i dont feel like making abstractions. i can later
pub struct Servo<'a> {
    channel: &'a mut hal::pwm::Channel<
        hal::pwm::Slice<hal::pwm::Pwm0, hal::pwm::FreeRunning>,
        hal::pwm::A,
    >,
    real_max: u16,
    real_min: u16,
    speed: f32,
    start: u8,
    end: u8,
    step: i16,
    pos: i16,
    last: u32,
    norm_factor: f32,
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

        // reset just cause
        channel.set_duty_cycle(0).unwrap();

        Self {
            channel,
            real_max,
            real_min,
            speed: f32::EPSILON,
            start: 0,
            end: 0,
            step: 1,
            pos: 0,
            last: 0,
            // norm = (max - min) * pos / u8::MAX + min
            // norm = min + (max - min) / u8::MAX * pos
            norm_factor: (real_max - real_min) as f32 / u8::MAX as f32, // compute once to save resources
        }
    }

    // could do move to and a bunch of steps to get to it, but i can just go to it now
    // i can break it apart later, but i dont need functions i wont use
    // pub fn move_to(&mut self, delay: &mut cortex_m::delay::Delay, to: u8, vel: u8)

    // convert to range of 0 - 1
    pub fn set_speed(&mut self, velocity: u8) {
        // stop possible div by 0
        if velocity == 0 {
            self.speed = f32::EPSILON;
            return;
        }

        self.speed = velocity as f32 / u8::MAX as f32;
    }

    pub fn set_amp(&mut self, amplitude: u8) {
        self.start = U8_HALF - amplitude;
        self.end = U8_HALF + amplitude;
    }

    pub fn update(&mut self, delay: &mut cortex_m::delay::Delay) {
        // first we must update sadly
        self.last += 1;
        delay.delay_ms(1);

        // TODO use delta instead, and delay outside or something
        // wait until it is time to do something
        if self.last <= (MAX_SPEED / self.speed) as u32 {
            // TODO not sure what happens on 2.5
            return;
        }
        self.last = 0;

        // check if we should change directions
        // TODO is this more efficient
        self.step = if self.pos >= self.end as i16 {
            -1
        } else if self.pos <= self.start as i16 {
            1
        } else {
            self.step
        };

        // convert pos to a duty cycle
        let duty = self.norm_factor * self.pos as f32 + self.real_min as f32;

        // cant do u16 because it would be too slow
        self.channel.set_duty_cycle(duty as u16).unwrap();
        self.pos += self.step;
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
