#include "ledc_pwm.h"
#include "driver/ledc.h"
#include "nvs_param.h"

#define LEDC_CH     LEDC_CHANNEL_0
#define LEDC_TIM    LEDC_TIMER_0

void ledc_init_pwm(void)
{
    ledc_timer_config_t t_cfg = {0};
    t_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    t_cfg.timer_num = LEDC_TIM;
    t_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    t_cfg.freq_hz = pwm_freq;
    ledc_timer_config(&t_cfg);

    ledc_channel_config_t ch_cfg = {0};
    ch_cfg.channel = LEDC_CH;
    ch_cfg.timer_sel = LEDC_TIM;
    ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_cfg.gpio_num = GPIO_NUM_3;
    ch_cfg.duty = 0;
    ch_cfg.hpoint = 0;
    ledc_channel_config(&ch_cfg);
}

void ledc_set_pwm_freq(uint32_t freq)
{
    pwm_freq = freq;
    ledc_timer_config_t t_cfg = {0};
    t_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    t_cfg.timer_num = LEDC_TIM;
    t_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    t_cfg.freq_hz = freq;
    ledc_timer_config(&t_cfg);
}

void ledc_set_pwm_duty(uint8_t duty)
{
    pwm_duty = duty;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);
}

void ledc_set_pwm_enable(bool en)
{
    pwm_switch = en;
    if(en)
    {
        ledc_set_pwm_duty(pwm_duty);
    }
    else
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH, 0);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH);
}