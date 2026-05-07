#ifndef LEDC_PWM_H
#define LEDC_PWM_H

void ledc_init_pwm(void);
void ledc_set_pwm_freq(uint32_t freq);
void ledc_set_pwm_duty(uint8_t duty);
void ledc_set_pwm_enable(bool en);

#endif