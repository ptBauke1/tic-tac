#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "stepper.hpp"

#define STEPS_PER_REV 200
#define STEPPER_DELAY 1400 // Speed of the displays, higher = slower
#define UPLOAD_OFFSET 5 // If the clock runs slow by a few seconds, increase this value
#define POLLING_DELAY 50 

stepper_t stepper_hour;
stepper_t stepper_dec;
stepper_t stepper_uni;

const uint8_t stepper_pins[][4] = {{6,5,4,3}, {15,14,8,7},{29,28,27,26}}; // [[hour] , [dec], [uni]]
const uint8_t endstop_pins[] = {1,0,2}; // [[hour] , [dec], [uni]]
const stepper_mode_t stepping_mode = power;
uint8_t speed = 20;

const uint8_t starting_digits[] = {0, 0, 9};
const unsigned int starting_offset[] = {110, 80, 265}; //// Starting offset should be increased until the top flap touches the front stop.

unsigned int stepper_pos[] = {0,0,0}; // Position in steps of each stepper
uint8_t drive_step[] = {0, 0, 0}; // Current drive step for each stepper - 0 to 7 for half-stepping

uint8_t uni_last = 0;


void step_with_rtc_uni(stepper_t *s, stepper_direction_t direction, datetime_t *time) {
  rtc_get_datetime(time);
  uint8_t uni = time->sec % 10;
  if (uni != uni_last) {
    stepper_rotate_steps(s, 200); //2048 = 360
  }
  uni_last = uni;
}

void step_with_rtc_dec(stepper_t *s, stepper_direction_t direction, datetime_t *time) {
  rtc_get_datetime(time);
  uint8_t uni = time->sec / 10;
  if (uni != uni_last) {
    stepper_rotate_steps(s, 125); //2048 = 360
  }
  uni_last = uni;
}

void step_with_rtc_hour(stepper_t *s, stepper_direction_t direction, datetime_t *time) {
  rtc_get_datetime(time);
  uint8_t uni = time->min % 12;
  if (uni != uni_last) {
    stepper_rotate_steps(s, 125); //2048 = 360
  }
  uni_last = uni;
}

void set_time (datetime_t *t){
  time_t current_time = time(NULL); // System time: number of seconds since 00:00, Jan 1 1970 UTC
  struct tm *day_time = localtime(&current_time);
  t->hour = (day_time->tm_hour - 3) % 12; //24hr para 12am/pm no fuso BRT
  t->min = day_time->tm_min;
  t->sec = day_time->tm_sec;
}

void display_home(stepper_t *s, stepper_direction_t direction){
  while(!gpio_get(s->endstop)){
    stepper_step_once(s, direction);
  }
}

int main() {
    stdio_init_all();

    char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];
    stepper_direction_t direction_uni = forward;
    stepper_direction_t direction_dec = backward;
    stepper_direction_t direction_hour = backward;
    datetime_t t = {
            .year  = 2024,
            .month = 10,
            .day   = 23,
            .dotw  = 3, // 0 is Sunday, so 5 is Friday
            .hour  = 17,
            .min   = 47,
            .sec   = 00
    };
    //set_time(&t);
    rtc_init();
    rtc_set_datetime(&t);
    
    stepper_init(&stepper_hour, stepper_pins[0][0], stepper_pins[0][1], stepper_pins[0][2], stepper_pins[0][3], STEPS_PER_REV, stepping_mode, endstop_pins[0]); 
    stepper_init(&stepper_dec, stepper_pins[1][0], stepper_pins[1][1], stepper_pins[1][2], stepper_pins[1][3], STEPS_PER_REV, stepping_mode, endstop_pins[1]); 
    stepper_init(&stepper_uni, stepper_pins[2][0], stepper_pins[2][1], stepper_pins[2][2], stepper_pins[2][3], STEPS_PER_REV, stepping_mode, endstop_pins[2]); 
    stepper_set_speed_rpm(&stepper_hour, speed);
    stepper_set_speed_rpm(&stepper_dec, speed);
    stepper_set_speed_rpm(&stepper_uni, speed);
    display_home(&stepper_uni,direction_uni);
    
    while (true) {
      step_with_rtc_uni(&stepper_uni, direction_uni, &t);
      step_with_rtc_dec(&stepper_dec, direction_dec, &t);
      step_with_rtc_hour(&stepper_hour, direction_hour, &t);
      sleep_ms(500);
}
}