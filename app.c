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

#if CONFIG_ESP_CONSOLE_USB_CDC
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#endif

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

#if CONFIG_ESP_CONSOLE_USB_CDC
static void usb_cdc_init(void)
{
    ESP_LOGI(TAG, "USB CDC initializing...");
    
    tinyusb_config_t tusb_cfg = {
        .descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
        return;
    }
    
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    
    ret = tusb_cdc_acm_init(&acm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CDC ACM init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "USB CDC initialized successfully");
}
#endif

void app_main(void)
{
    // 最基础的测试：仅输出最简单的信息
    printf("\r\n*** BASIC TEST START ***\r\n");
    fflush(stdout);
    
    // 延迟一下，看是否能输出
    for(int i = 0; i < 5; i++) {
        printf("Test output #%d\r\n", i+1);
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    printf("Basic test completed.\r\n");
    fflush(stdout);
    
    // 如果能看到这个输出，说明问题在其他初始化代码
    // 注释掉所有可能导致问题的代码
    /*
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

#if CONFIG_ESP_CONSOLE_USB_CDC
    // 初始化USB CDC控制台
    usb_cdc_init();
    // 等待USB枚举完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "System starting with USB CDC console");
#endif

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
    */
}
}