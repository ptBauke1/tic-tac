#include <stdio.h>
#include "pico/stdlib.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

typedef struct {
    int16_t position;
    uint64_t step_delay_us;
    uint32_t gpio_mask;
    uint32_t stepping_sequence[4];
    float step_angle;
    uint16_t steps_per_revolution;
} stepper_t;

typedef enum {
    single,
    power
} stepper_mode_t;

typedef enum {
    forward = 1,
    backward = -1
} stepper_direction_t;



stepper_t stepper;
const uint8_t stepper_pin_1A = 5;
const uint8_t stepper_pin_1B = 6;
const uint8_t stepper_pin_2A = 7;
const uint8_t stepper_pin_2B = 8;
const uint16_t stepper_steps_per_revolution = 200;
const stepper_mode_t stepping_mode = single;
uint8_t speed = 20;

void stepper_init(stepper_t *s, uint8_t pin_1A, uint8_t pin_1B,
                  uint8_t pin_2A, uint8_t pin_2B,
                  uint16_t steps_per_revolution,
                  stepper_mode_t stepping_mode) {
    // Initialise GPIO. Use a bitmask to manipulate all pins at the same time.
    s->gpio_mask = (1 << pin_1A) | (1 << pin_1B) |
                   (1 << pin_2A) | (1 << pin_2B);
    gpio_init_mask(s->gpio_mask);
    gpio_set_dir_out_masked(s->gpio_mask);

    // Initialise stepping parameters. The stepping sequences are
    // bitmasks to activate the motor pins. These firing sequences can
    // be "single" or "power" (After Scherz and Monk 2013, Fig 14.8):
    //
    // Single stepping      Power stepping
    //      Coil                 Coil
    // Step 1A 1B 2A 2B     Step 1A 1B 2A 2B
    //    0  1  0  0  0        0  1  0  1  0
    //    1  0  0  1  0        1  0  1  1  0
    //    2  0  1  0  0        2  0  1  0  1
    //    3  0  0  0  1        3  1  0  0  1
    if (stepping_mode == single){
        s->stepping_sequence[0] = 1 << pin_1A;
        s->stepping_sequence[1] = 1 << pin_2A;
        s->stepping_sequence[2] = 1 << pin_1B;
        s->stepping_sequence[3] = 1 << pin_2B;
    } else if (stepping_mode == power) {
        s->stepping_sequence[0] = (1 << pin_1A) | (1 << pin_2A);
        s->stepping_sequence[1] = (1 << pin_1B) | (1 << pin_2A);
        s->stepping_sequence[2] = (1 << pin_1B) | (1 << pin_2B);
        s->stepping_sequence[3] = (1 << pin_1A) | (1 << pin_2B);
    }

    s->steps_per_revolution = steps_per_revolution;
    s->step_angle = 360.0 / steps_per_revolution;

    // Initiallize motor at position 0.
    s->position = 0;
    gpio_put_masked(s->gpio_mask, s->stepping_sequence[0]);
}

void stepper_set_speed_rpm(stepper_t *s, uint8_t rpm){
    // Delay (in us) between steps
    s->step_delay_us = 6e7 / s->steps_per_revolution / rpm;
}

void stepper_step_once(stepper_t *s, stepper_direction_t direction) {
    s->position += direction;
    if (s->position == s->steps_per_revolution) {
        s->position = 0;
    } else if (s->position < 0) {
        s->position = s->steps_per_revolution - 1;
    }
    gpio_put_masked(s->gpio_mask, s->stepping_sequence[s->position % 4]);
}

void stepper_release(stepper_t *s) {
    gpio_put_masked(s->gpio_mask, 0);
}

void stepper_rotate_steps(stepper_t *s, int16_t steps) {
    stepper_direction_t direction;
    if (steps > 0) {
        direction = forward;
    } else {
        direction = backward;
    }
    
    while (true) {
        stepper_step_once(s, direction);
        steps -= direction;
        if (steps == 0) break;
        busy_wait_us(s->step_delay_us);
    }
}

void stepper_rotate_degrees(stepper_t *s, float degrees) {
    int16_t steps = degrees / s->step_angle;
    stepper_rotate_steps(s, steps);
}

int main() {
    stdio_init_all();

    stepper_init(&stepper, stepper_pin_1A, stepper_pin_1B,
                 stepper_pin_2A, stepper_pin_2B,
                 stepper_steps_per_revolution, stepping_mode);
    stepper_set_speed_rpm(&stepper, speed);

    while(1) {
        // Rotate 3/4 of a turn.
        stepper_rotate_degrees(&stepper, 270);
        sleep_ms(500);

        // Now rotate these many steps in the oposite direction
        stepper_rotate_steps(&stepper, -45);
        sleep_ms(500);

        // Increase the speed and rotate 360 degrees
        speed = 50;
        stepper_set_speed_rpm(&stepper, speed);
        stepper_rotate_degrees(&stepper, 360);

        // Release the coils and sleep for a while. You can check that
        // the coils are not energised by moving the rotor manually:
        // there should be little resistance.
        stepper_release(&stepper);
        sleep_ms(4000);

        // Decrease the speed
        speed = 15;
        stepper_set_speed_rpm(&stepper, speed);
    }
    return 0;
}