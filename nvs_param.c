#include "nvs_param.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS";

char wifi_ssid[32]   = {0};
char wifi_pswd[64]   = {0};
char mqtt_uri[64]    = {0};
char mqtt_user[32]   = {0};
char mqtt_pass[32]   = {0};

uint32_t pwm_freq    = 200000;
uint8_t  pwm_duty    = 128;
bool     pwm_switch  = false;

void nvs_read_all_param(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("user_cfg", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s, using defaults", esp_err_to_name(err));
        return;
    }

    size_t len = sizeof(wifi_ssid);
    err = nvs_get_str(handle, "ssid", wifi_ssid, &len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved SSID");
        wifi_ssid[0] = '\0';
    }
    
    len = sizeof(wifi_pswd);
    err = nvs_get_str(handle, "pswd", wifi_pswd, &len);
    if (err != ESP_OK) {
        wifi_pswd[0] = '\0';
    }
    
    len = sizeof(mqtt_uri);
    err = nvs_get_str(handle, "mqtt_uri", mqtt_uri, &len);
    if (err != ESP_OK) {
        mqtt_uri[0] = '\0';
    }
    
    len = sizeof(mqtt_user);
    err = nvs_get_str(handle, "mqtt_user", mqtt_user, &len);
    if (err != ESP_OK) {
        mqtt_user[0] = '\0';
    }
    
    len = sizeof(mqtt_pass);
    err = nvs_get_str(handle, "mqtt_pass", mqtt_pass, &len);
    if (err != ESP_OK) {
        mqtt_pass[0] = '\0';
    }

    err = nvs_get_u32(handle, "freq", &pwm_freq);
    if (err != ESP_OK) {
        pwm_freq = 200000;  // 默认200kHz
    }
    
    err = nvs_get_u8(handle, "duty", &pwm_duty);
    if (err != ESP_OK) {
        pwm_duty = 128;  // 默认50%占空比
    }
    
    uint8_t en = 0;
    err = nvs_get_u8(handle, "en", &en);
    if (err != ESP_OK) {
        en = 0;
    }
    pwm_switch = en ? true : false;

    nvs_close(handle);
    
    ESP_LOGI(TAG, "Params loaded: freq=%lu, duty=%d, enable=%d", 
             (unsigned long)pwm_freq, pwm_duty, pwm_switch);
}

void nvs_save_all_param(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("user_cfg", NVS_READWRITE, &handle);
    if(err != ESP_OK) return;

    nvs_set_str(handle, "ssid", wifi_ssid);
    nvs_set_str(handle, "pswd", wifi_pswd);
    nvs_set_str(handle, "mqtt_uri", mqtt_uri);
    nvs_set_str(handle, "mqtt_user", mqtt_user);
    nvs_set_str(handle, "mqtt_pass", mqtt_pass);

    nvs_set_u32(handle, "freq", pwm_freq);
    nvs_set_u8(handle, "duty", pwm_duty);
    nvs_set_u8(handle, "en", pwm_switch ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);
}

void nvs_erase_factory_all(void)
{
    nvs_flash_erase();
}