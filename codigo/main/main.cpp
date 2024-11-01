#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

#define STEPS_PER_REV 2048
#define STEPPER_DELAY 1400 // Speed of the displays, higher = slower
#define UPLOAD_OFFSET 5 // If the clock runs slow by a few seconds, increase this value
#define POLLING_DELAY 50 
#define ENDSTOP_DEBOUNCE_READS 3 // Number of repeated endstop reads during homing


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

stepper_t stepper_hour;
stepper_t stepper_dec;
stepper_t stepper_uni;

const byte stepper_pins[][4] = {{5,7,6,8}, {9,11,10,12},{13,15,14,16}}; // [[hour] , [dec], [uni]]
const byte endstop_pins[] = {21,20,20};
const stepper_mode_t stepping_mode = power;
uint8_t speed = 50;

const byte starting_digits[] = {0, 0, 9};
const unsigned int starting_offset[] = {110, 80, 265}; //// Starting offset should be increased until the top flap touches the front stop.

unsigned int stepper_pos[] = {0,0,0}; // Position in steps of each stepper
byte drive_step[] = {0, 0, 0}; // Current drive step for each stepper - 0 to 7 for half-stepping

void e_stop(){
    abort();
}

void disable_stepper(const bythe stepper_num){
    gpio_put(stepper_pins[stepper_num][0], LOW);
    gpio_put(stepper_pins[stepper_num][1], LOW);
    gpio_put(stepper_pins[stepper_num][2], LOW);
    gpio_put(stepper_pins[stepper_num][3], LOW);
}

void half_step(const byte stepper_num){
    const byte pos = drive_step[stepper_num];
    gpio_put(stepper_pins[stepper_num][0], pos < 3);
    gpio_put(stepper_pins[stepper_num][1], ((pos + 6) % 8) < 3);
    gpio_put(stepper_pins[stepper_num][2], ((pos + 4) % 8) < 3);
    gpio_put(stepper_pins[stepper_num][3], ((pos + 2) % 8) < 3);
    drive_step[stepper_num] = (pos + 1) % 8;
}

void step_num(const byte stepper_num, unsigned int steps, const unsigned int wait){
    stepper_pos[stepper_num] = (stepper_pos[stepper_num] + steps) % STEPS_PER_REV;
    while(steps > 0) {
        half_step(stepper_num);
        steps--;  
        delayMicroseconds(wait);
  }
}

void step_to_home(const byte stepper_num, const unsigned int wait) {  
  unsigned int total_steps = 0;
  byte endstop_repeats = 0;
  
  // Step until endstop reads low ENDSTOP_DEBOUNCE_READS times in a row
  while(endstop_repeats < ENDSTOP_DEBOUNCE_READS) {
    endstop_repeats = digitalRead(endstop_pins[stepper_num]) == LOW ? endstop_repeats + 1 : 0;
    
    half_step(stepper_num);    
    total_steps++;
    if(total_steps > STEPS_PER_REV * 2u) {
      disable_stepper(stepper_num);
      e_stop();
    } 
    delayMicroseconds(wait);
  }
  endstop_repeats = 0;
  
  // Step until endstop reads high ENDSTOP_DEBOUNCE_READS times in a row
  while(endstop_repeats < ENDSTOP_DEBOUNCE_READS) {
    endstop_repeats = digitalRead(endstop_pins[stepper_num]) == HIGH ? endstop_repeats + 1 : 0;
    
    half_step(stepper_num);
    total_steps++;
    if(total_steps > STEPS_PER_REV * 2u) {
      disable_stepper(stepper_num);
      e_stop();
    }
    delayMicroseconds(wait);
  }

  stepper_pos[stepper_num] = 0; 
}

void step_to_position(const byte stepper_num, unsigned int target_pos, const unsigned int wait) {
  if (target_pos == stepper_pos[stepper_num]) {
    return;
  }

  // Limit target position to between 0 and STEPS_PER_REV-1
  target_pos %= STEPS_PER_REV;
  
  if (target_pos < stepper_pos[stepper_num]) {
    step_to_home(stepper_num, wait);
    step_num(stepper_num, target_pos, wait);
  } else {
    step_num(stepper_num, target_pos - stepper_pos[stepper_num], wait);
  }
}

void step_to_digit(const byte stepper_num, const byte digit, const unsigned int wait) {
  // The ones display has 10 flaps, the others have 12 flaps
  const byte num_flaps = (stepper_num == 2) ? 10 : 12;
  
  const byte num_digits = (stepper_num == 0) ? 12 : (stepper_num == 1) ? 6 : 10;

  const unsigned int target_pos = starting_offset[stepper_num] + (unsigned int)((num_digits + digit - starting_digits[stepper_num]) % num_digits) * STEPS_PER_REV/num_flaps;
  
  // The tens display has 2 full sets of digits, so we'll need to step to the closest target digit.
  if(stepper_num == 1) {
    // The repeated digit is offset by a half-rotation from the first target.
    const unsigned int second_target = target_pos + STEPS_PER_REV/2;

    // If the current position is between the two target positions, step to the second target position, as it's the closest given that we can't step backwards.
    // Otherwise, step to the first target position.
    if(stepper_pos[stepper_num] > target_pos && stepper_pos[stepper_num] <= second_target) {
      step_to_position(stepper_num, second_target, wait);
    } else {
      step_to_position(stepper_num, target_pos, wait);
    }
  } else {
    // The ones and hour displays only have a single set of digits, so simply step to the target position.
    step_to_position(stepper_num, target_pos, wait);
  }
  
  disable_stepper(stepper_num);
}

void stepper_init(stepper_t *s, uint8_t pin_1A, uint8_t pin_1B, uint8_t pin_2A, uint8_t pin_2B, uint16_t steps_per_revolution, stepper_mode_t stepping_mode) {
    // Initialise GPIO. Use a bitmask to manipulate all pins at the same time.
    s->gpio_mask = (1 << pin_1A) | (1 << pin_1B) | (1 << pin_2A) | (1 << pin_2B);
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

    char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];

    // Start on Friday 5th of June 2020 15:45:00
    datetime_t t = {
            .year  = 2024,
            .month = 10,
            .day   = 23,
            .dotw  = 3, // 0 is Sunday, so 5 is Friday
            .hour  = 17,
            .min   = 47,
            .sec   = 00
    };

    // Start the RTC
    rtc_init();
    rtc_set_datetime(&t);
    
    for (int i = 0; i < 3; i++){
        gpio_init(endstop_pins[i]);
        gpio_set_dir(endstop_pins[i], INPUT);
    }
    
    stepper_init(stepper_hour, stepper_pins[0][0], stepper_pins[0][1], stepper_pins[0][2], stepper_pins[0][3], STEPS_PER_REV, stepping_mode) 
    stepper_init(stepper_dec, stepper_pins[1][0], stepper_pins[1][1], stepper_pins[1][2], stepper_pins[1][3], STEPS_PER_REV, stepping_mode) 
    stepper_init(stepper_uni, stepper_pins[2][0], stepper_pins[2][1], stepper_pins[2][2], stepper_pins[2][3], STEPS_PER_REV, stepping_mode) 
    
    unsigned int total_steps = 0;
    if(endstop_pins[1] == endstop_pins[2]) {
        while(digitalRead(endstop_pins[1]) == LOW) {
            step_num(1, 200, STEPPER_DELAY);
            disable_stepper(1);

      // Still pressed, try the other display
            if(digitalRead(endstop_pins[1]) == LOW) {
                step_num(2, 200, STEPPER_DELAY);  
                disable_stepper(2);
            }

            // Similar to homing, if a max number of steps is reached, the endstop is assumed to have failed and the program aborts.
            total_steps += 200;
            if(total_steps > STEPS_PER_REV) {
                e_stop();
            }
        }
    }
    step_to_home(2, STEPPER_DELAY);
    step_to_digit(2, starting_digits[2], STEPPER_DELAY);
    step_to_home(1, STEPPER_DELAY);
    step_to_digit(1, starting_digits[1], STEPPER_DELAY);
    step_to_home(0, STEPPER_DELAY);
    step_to_digit(0, starting_digits[0], STEPPER_DELAY);

    // clk_sys is >2000x faster than clk_rtc, so datetime is not updated immediately when rtc_get_datetime() is called.
    // The delay is up to 3 RTC clock cycles (which is 64us with the default clock settings)
    sleep_us(64);

    // Print the time
    while (true) {
        rtc_get_datetime(&t);
        uint8_t hr = t.hour;
        uint8_t dec = static_cast < uint8_t > t.min / 10;
        uint8_t uni = t.min % 10;

        step_to_digit(2, ones, STEPPER_DELAY);
        tep_to_digit(1, tens, STEPPER_DELAY);
        step_to_digit(0, hr, STEPPER_DELAY);
    }
}