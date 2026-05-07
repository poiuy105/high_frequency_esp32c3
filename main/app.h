#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

#define LED_PIN     GPIO_NUM_4
#define KEY_PIN     GPIO_NUM_9
#define PWM_PIN     GPIO_NUM_3

void app_main(void);

#endif