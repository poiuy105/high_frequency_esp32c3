#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "nvs_param.h"
#include "ledc_pwm.h"
#include "wifi_airkiss.h"
#include "mqtt_ha_harmony.h"
#include "key.h"
#include "rtc_boot_reset.h"

// ESP32-C6 控制台配置
// UART0: GPIO8(RX), GPIO9(TX) - 硬件串口，可靠输出
// USB Serial/JTAG: USB接口 - 用于调试

static const char *TAG = "MAIN";

void led_status_task(void *arg)
{
    while(1)
    {
        if(!wifi_is_connected())
        {
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else if(!mqtt_is_connected())
        {
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
        else
        {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
    }
}

void status_publish_task(void *arg)
{
    while(1)
    {
        if(mqtt_is_connected())
        {
            mqtt_publish_device_status();
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // 立即输出启动信息，确认控制台工作
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C6 System Starting...");
    ESP_LOGI(TAG, "Chip: %s, %d cores", esp_get_model_info(), esp_get_cpu_count());
    ESP_LOGI(TAG, "Console: UART0 (RX=GPIO8, TX=GPIO9)");
    printf("=== ESP32-C6 Firmware Started ===\r\n");
    fflush(stdout);

    // 快速上电连击检测恢复出厂
    boot_count_reset_check();

    // NVS初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS initialized successfully");

    // 读取所有掉电保存参数
    nvs_read_all_param();
    ESP_LOGI(TAG, "Parameters loaded from NVS");

    // GPIO4 状态灯 低电平亮
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << LED_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, 1);

    // GPIO9 按键 内部上拉
    io_conf.pin_bit_mask = (1ULL << KEY_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "GPIO initialized: LED=%d, KEY=%d", LED_PIN, KEY_PIN);

    // PWM初始化 上电默认关闭低电平
    ledc_init_pwm();
    ledc_set_pwm_enable(pwm_switch);
    ESP_LOGI(TAG, "PWM initialized");

    // 按键初始化
    key_init();
    ESP_LOGI(TAG, "Key initialized");

    // 小程序AirKiss配网
    wifi_airkiss_start();
    ESP_LOGI(TAG, "WiFi AirKiss started");

    // MQTT连接鸿蒙+HA
    mqtt_client_init();
    ESP_LOGI(TAG, "MQTT client initialized");

    // 状态灯任务
    xTaskCreate(led_status_task, "led_task", 2048, NULL, 2, NULL);
    // 设备状态定时上报
    xTaskCreate(status_publish_task, "status_task", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "All tasks started. System running.");
}
