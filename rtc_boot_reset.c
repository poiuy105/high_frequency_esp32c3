#include "rtc_boot_reset.h"
#include "esp_rtc.h"
#include "nvs_param.h"
#include "esp_system.h"

#define BOOT_LIMIT_COUNT    10
#define BOOT_LIMIT_TIME_MS 15000

void boot_count_reset_check(void)
{
    // RTC计时：15秒内连续上电10次 → 清空NVS恢复出厂
    uint32_t last_time = esp_rtc_get_time_us();
    static uint8_t boot_cnt = 0;

    if(last_time < BOOT_LIMIT_TIME_MS*1000)
        boot_cnt++;
    else
        boot_cnt = 0;

    if(boot_cnt >= BOOT_LIMIT_COUNT)
    {
        nvs_erase_factory_all();
        esp_restart();
    }
}