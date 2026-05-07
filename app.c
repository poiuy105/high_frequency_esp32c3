#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "nvs_param.h"
#include "ledc_pwm.h"
#include "wifi_airkiss.h"
#include "mqtt_ha_harmony.h"
#include "key.h"
#include "rtc_boot_reset.h"
#include "usb_serial_jtag.h"

// ESP32-C3 使用内置 USB Serial/JTAG 控制器
// 需要显式初始化驱动以确保控制台输出正常工作

static const char *TAG = "MAIN";

// USB Serial/JTAG 初始化函数
static void usb_serial_jtag_console_init(void)
{
    // 安装 USB Serial/JTAG 驱动
    // 这确保控制台输出正确路由到 USB
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    usb_serial_jtag_driver_install(&usb_config);

    // 等待USB枚举完成（给主机时间识别设备）
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

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
    // 首先初始化 USB Serial/JTAG 控制台
    // 必须在所有日志输出之前完成
    usb_serial_jtag_console_init();

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

    ESP_LOGI(TAG, "System starting with USB Serial/JTAG console");
    printf("=== ESP32-C3 System Started ===\r\n");

    // 读取所有掉电保存参数
    nvs_read_all_param();
    
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

    // PWM初始化 上电默认关闭低电平
    ledc_init_pwm();
    ledc_set_pwm_enable(pwm_switch);

    // 按键初始化
    key_init();

    // 小程序AirKiss配网
    wifi_airkiss_start();

    // MQTT连接鸿蒙+HA
    mqtt_client_init();

    // 状态灯任务
    xTaskCreate(led_status_task, "led_task", 2048, NULL, 2, NULL);
    // 设备状态定时上报
    xTaskCreate(status_publish_task, "status_task", 2048, NULL, 1, NULL);
}