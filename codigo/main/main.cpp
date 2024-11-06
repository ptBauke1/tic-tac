#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "stepper.hpp"

#define STEPS_PER_REV 2048
#define STEPPER_DELAY 1400 // Speed of the displays, higher = slower


stepper_t stepper_hour;
stepper_t stepper_dec;
stepper_t stepper_uni;

datetime_t t = {
  .year  = 2024,
  .month = 10,
  .day   = 23,
  .dotw  = 3, // 0 is Sunday, so 5 is Friday
  .hour  = 17,
  .min   = 58,
  .sec   = 00
};

const uint8_t stepper_pins[][4] = {{6,5,4,3}, {15,14,8,7},{29,28,27,26}}; // [[hour] , [dec], [uni]] {IN1,IN3,IN2,IN4}
const uint8_t endstop_pins[] = {1,0,2}; // [[hour] , [dec], [uni]]
const stepper_mode_t stepping_mode = power;
uint8_t speed = 30;

const uint8_t starting_digits[] = {0, 3, 1};
const unsigned int starting_offset[] = {2, 40, 50}; //// Starting offset should be increased until the top flap touches the front stop.

unsigned int stepper_pos[] = {0,0,0}; // Position in steps of each stepper
uint8_t drive_step[] = {0, 0, 0}; // Current drive step for each stepper - 0 to 7 for half-stepping

uint8_t uni_last = 0;


void display_home(stepper_t *s){
  while(!gpio_get(s->endstop)){
    stepper_step_once(s);
    sleep_ms(10);
  }
  stepper_rotate_steps(s, starting_offset[s->index]);
  sleep_ms(10);
}
/*
void step_with_rtc(stepper_t *s) {
  rtc_get_datetime(&t);
  uint8_t uni = t->min / 10;
  uint8_t dec = t->min % 10;
  uint8_t hour = t->hour % 12;
  //125
  if (s->index == 0)
}
*/

// Take the specified amount of steps for a stepper connected to the specified pins, with a
// specified delay (in microseconds) between each step.
void step_num(stepper_t *s, unsigned int steps, const unsigned int wait) {
  stepper_pos[s->index] = (stepper_pos[s->index] + steps) % STEPS_PER_REV;
  while(steps > 0) {
    stepper_step_once(s);
    steps--;  
    sleep_ms(wait);
  }
}

void step_to_position(stepper_t *s, unsigned int target_pos, const unsigned int wait) {
  if (target_pos == stepper_pos[s->index]) {
    return;
  }

  // Limit target position to between 0 and STEPS_PER_REV-1
  target_pos %= STEPS_PER_REV;
  
  if (target_pos < stepper_pos[s->index]) {
    display_home(s);
    step_num(s, target_pos, wait);
  } else {
    step_num(s, target_pos - stepper_pos[s->index], wait);
  }
}

void step_to_digit(stepper_t *s, const uint8_t digit, const unsigned int wait) {
  const uint8_t num_flaps = (s->index == 2) ? 10 : 12;
  const uint8_t num_digits = (s->index == 0) ? 12 : (s->index == 1) ? 6 : 10;

  const unsigned int target_pos = starting_offset[s->index] + (unsigned int)((num_digits + digit - starting_digits[s->index]) % num_digits) * STEPS_PER_REV/num_flaps;
  // The tens display has 2 full sets of digits, so we'll need to step to the closest target digit.
  if(s->index == 1) {
    // The repeated digit is offset by a half-rotation from the first target.
    const unsigned int second_target = target_pos + STEPS_PER_REV/2;

    // If the current position is between the two target positions, step to the second target position, as it's the closest given that we can't step backwards.
    // Otherwise, step to the first target position.
    if(stepper_pos[s->index] > target_pos && stepper_pos[s->index] <= second_target) {
      step_to_position(s, second_target, wait);
    } else {
      step_to_position(s, target_pos, wait);
    }
  } else {
    // The ones and hour displays only have a single set of digits, so simply step to the target position.
    step_to_position(s, target_pos, wait);
  }
  
}

void set_time (datetime_t *t){
  time_t current_time = time(NULL); // System time: number of seconds since 00:00, Jan 1 1970 UTC
  struct tm *day_time = localtime(&current_time);
  t->hour = (day_time->tm_hour - 3) % 12; //24hr para 12am/pm no fuso BRT
  t->min = day_time->tm_min;
  t->sec = day_time->tm_sec;
}

int main() {
    stdio_init_all();

    char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];

    //set_time(&t);
    rtc_init();
    rtc_set_datetime(&t);
    
    stepper_init(&stepper_hour, stepper_pins[0][0], stepper_pins[0][1], stepper_pins[0][2], stepper_pins[0][3], STEPS_PER_REV, stepping_mode, endstop_pins[0], 0, backward); 
    stepper_init(&stepper_dec, stepper_pins[1][0], stepper_pins[1][1], stepper_pins[1][2], stepper_pins[1][3], STEPS_PER_REV, stepping_mode, endstop_pins[1], 1, backward); 
    stepper_init(&stepper_uni, stepper_pins[2][0], stepper_pins[2][1], stepper_pins[2][2], stepper_pins[2][3], STEPS_PER_REV, stepping_mode, endstop_pins[2], 2, forward); 
    stepper_set_speed_rpm(&stepper_hour, speed);
    stepper_set_speed_rpm(&stepper_dec, speed);
    stepper_set_speed_rpm(&stepper_uni, speed);
    gpio_init(stepper_uni.endstop);
    gpio_pull_down(stepper_uni.endstop);
    gpio_set_dir(stepper_uni.endstop, GPIO_IN);

    gpio_init(stepper_dec.endstop);
    gpio_pull_down(stepper_dec.endstop);
    gpio_set_dir(stepper_dec.endstop, GPIO_IN);

    gpio_init(stepper_hour.endstop);
    gpio_pull_down(stepper_hour.endstop);
    gpio_set_dir(stepper_hour.endstop, GPIO_IN);

    display_home(&stepper_uni);
    step_to_digit(&stepper_uni, starting_digits[stepper_uni.index], 20);
    sleep_ms(20);

    display_home(&stepper_dec);
    step_to_digit(&stepper_dec, starting_digits[stepper_dec.index], 20);
    sleep_ms(20);

    display_home(&stepper_hour);
    step_to_digit(&stepper_hour, starting_digits[stepper_hour.index], 20);
    sleep_ms(20);

    while (true) {
      rtc_get_datetime(&t);
      uint8_t uni = t.min % 10;
      uint8_t dec = t.min / 10;
      uint8_t hour = t.hour % 12;

      step_to_digit(&stepper_uni, uni, 20);
      step_to_digit(&stepper_dec, dec, 20);
      step_to_digit(&stepper_hour, hour, 20);
      sleep_ms(20);
}
}