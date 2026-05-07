#ifndef NVS_PARAM_H
#define NVS_PARAM_H

#include <stdint.h>
#include <stdbool.h>

extern char wifi_ssid[32];
extern char wifi_pswd[64];

extern char mqtt_uri[64];
extern char mqtt_user[32];
extern char mqtt_pass[32];

extern uint32_t pwm_freq;
extern uint8_t  pwm_duty;
extern bool     pwm_switch;

void nvs_read_all_param(void);
void nvs_save_all_param(void);
void nvs_erase_factory_all(void);

#endif