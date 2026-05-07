#include "wifi_airkiss.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "esp_event.h"
#include "nvs_param.h"
#include <string.h>

bool wifi_connected = false;

bool wifi_is_connected(void)
{
    return wifi_connected;
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if(base == WIFI_EVENT_STA_CONNECTED)
        wifi_connected = true;

    if(base == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        esp_wifi_connect();
    }
}

void wifi_airkiss_start(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_smartconfig_set_type(SC_TYPE_AIRKISS);
    smartconfig_start_config_t sc_cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&sc_cfg);
}