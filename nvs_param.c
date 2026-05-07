#include "nvs_param.h"
#include "nvs_flash.h"
#include <string.h>

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
    if(err != ESP_OK) return;

    nvs_get_str(handle, "ssid", wifi_ssid, sizeof(wifi_ssid));
    nvs_get_str(handle, "pswd", wifi_pswd, sizeof(wifi_pswd));
    nvs_get_str(handle, "mqtt_uri", mqtt_uri, sizeof(mqtt_uri));
    nvs_get_str(handle, "mqtt_user", mqtt_user, sizeof(mqtt_user));
    nvs_get_str(handle, "mqtt_pass", mqtt_pass, sizeof(mqtt_pass));

    nvs_get_u32(handle, "freq", &pwm_freq);
    nvs_get_u8(handle, "duty", &pwm_duty);
    uint8_t en = 0;
    nvs_get_u8(handle, "en", &en);
    pwm_switch = en ? true : false;

    nvs_close(handle);
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