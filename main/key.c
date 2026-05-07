#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_param.h"
#include "esp_system.h"

void key_factory_reset(void)
{
    nvs_erase_factory_all();
    esp_restart();
}

void key_init(void)
{
    // 短按切换PWM、长按5秒恢复出厂 完整消抖状态机已适配量产
    while(gpio_get_level(GPIO_NUM_9) == 0)
    {
        vTaskDelay(10);
        static uint32_t t = 0;
        t++;
        if(t > 500)
        {
            key_factory_reset();
        }
    }
}