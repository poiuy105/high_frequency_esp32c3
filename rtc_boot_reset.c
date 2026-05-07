#include "rtc_boot_reset.h"
#include "esp_system.h"
#include "nvs_param.h"

#define BOOT_LIMIT_COUNT    10
#define BOOT_LIMIT_TIME_MS 15000

void boot_count_reset_check(void)
{
    // RTC计时：15秒内连续上电10次 → 清空NVS恢复出厂
    esp_reset_reason_t reason = esp_reset_reason();
    static uint8_t boot_cnt = 0;

    // 如果是上电复位或深度睡眠唤醒，增加计数
    if (reason == ESP_RST_POWERON || reason == ESP_RST_DEEPSLEEP) {
        boot_cnt++;
    } else {
        boot_cnt = 0;
    }

    if (boot_cnt >= BOOT_LIMIT_COUNT)
    {
        nvs_erase_factory_all();
        esp_restart();
    }
}