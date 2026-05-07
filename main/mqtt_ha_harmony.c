#include "mqtt_ha_harmony.h"
#include "mqtt_client.h"
#include "nvs_param.h"
#include "ledc_pwm.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

bool mqtt_online = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "MQTT";

#define MQTT_TOPIC_SWITCH   "harmony/pwm/switch"
#define MQTT_TOPIC_FREQ     "harmony/pwm/freq"
#define MQTT_TOPIC_DUTY     "harmony/pwm/duty"
#define MQTT_TOPIC_STATE    "harmony/pwm/state"

bool mqtt_is_connected(void)
{
    return mqtt_online;
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t evt = data;

    switch(id)
    {
        case MQTT_EVENT_CONNECTED:
            mqtt_online = true;
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_subscribe(evt->client, MQTT_TOPIC_SWITCH, 0);
            esp_mqtt_client_subscribe(evt->client, MQTT_TOPIC_FREQ, 0);
            esp_mqtt_client_subscribe(evt->client, MQTT_TOPIC_DUTY, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_online = false;
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data received");
            if(strstr(evt->topic, MQTT_TOPIC_SWITCH))
            {
                bool en = strstr(evt->data, "ON") ? true : false;
                ledc_set_pwm_enable(en);
                nvs_save_all_param();
            }
            else if(strstr(evt->topic, MQTT_TOPIC_FREQ))
            {
                uint32_t freq = atoi(evt->data);
                if(freq>=10000 && freq<=300000)
                {
                    ledc_set_pwm_freq(freq);
                    nvs_save_all_param();
                }
            }
            else if(strstr(evt->topic, MQTT_TOPIC_DUTY))
            {
                uint8_t duty = atoi(evt->data);
                if(duty<=255)
                {
                    ledc_set_pwm_duty(duty);
                    nvs_save_all_param();
                }
            }
            break;
    }
}

void mqtt_client_init(void)
{
    // 检查MQTT URI是否已配置
    if (mqtt_uri[0] == '\0') {
        ESP_LOGW(TAG, "MQTT URI not configured, skipping MQTT initialization");
        ESP_LOGW(TAG, "Please configure MQTT settings via NVS or code");
        return;
    }

    ESP_LOGI(TAG, "Initializing MQTT client...");
    ESP_LOGI(TAG, "Broker: %s", mqtt_uri);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = mqtt_uri,
        .credentials.username = mqtt_user,
        .credentials.authentication.password = mqtt_pass,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT client started");
}

void mqtt_publish_device_status(void)
{
    if (mqtt_client == NULL || !mqtt_online) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "freq:%lu,duty:%d,en:%d",
             (unsigned long)pwm_freq, pwm_duty, pwm_switch);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATE, buf, 0, 0, 0);
}
