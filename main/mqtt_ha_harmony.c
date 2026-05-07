#include "mqtt_ha_harmony.h"
#include "mqtt_client.h"
#include "nvs_param.h"
#include "ledc_pwm.h"
#include "wifi_prov.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

bool mqtt_online = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "MQTT";

// 设备唯一标识符
static char device_identifier[24] = {0};

// Discovery topics
static char disc_switch_config[128];
static char disc_sensor_freq_config[128];
static char disc_sensor_duty_config[128];
static char disc_sensor_state_config[128];
static char disc_select_freq_config[128];
static char disc_select_duty_config[128];

// State topics
static char state_switch_topic[64];
static char state_freq_topic[64];
static char state_duty_topic[64];

// Command topics
static char cmd_switch_topic[64];
static char cmd_freq_topic[64];
static char cmd_duty_topic[64];

// Payload buffers - 增加大小以避免截断
static char device_info[512];
static char switch_payload[1024];
static char freq_sensor_payload[1024];
static char duty_sensor_payload[1024];
static char state_sensor_payload[1024];
static char freq_select_payload[1280];
static char duty_select_payload[1536];

// 获取设备唯一标识符
static void get_device_identifier(void)
{
    if (device_identifier[0] != '\0') {
        return;
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_identifier, sizeof(device_identifier), "esp32c6_hf_%02x%02x%02x",
             mac[3], mac[4], mac[5]);

    // 构建所有topic
    snprintf(disc_switch_config, sizeof(disc_switch_config),
             "homeassistant/switch/%s_switch/config", device_identifier);
    snprintf(disc_sensor_freq_config, sizeof(disc_sensor_freq_config),
             "homeassistant/sensor/%s_freq/config", device_identifier);
    snprintf(disc_sensor_duty_config, sizeof(disc_sensor_duty_config),
             "homeassistant/sensor/%s_duty/config", device_identifier);
    snprintf(disc_sensor_state_config, sizeof(disc_sensor_state_config),
             "homeassistant/sensor/%s_state/config", device_identifier);
    snprintf(disc_select_freq_config, sizeof(disc_select_freq_config),
             "homeassistant/select/%s_freq/config", device_identifier);
    snprintf(disc_select_duty_config, sizeof(disc_select_duty_config),
             "homeassistant/select/%s_duty/config", device_identifier);

    snprintf(state_switch_topic, sizeof(state_switch_topic),
             "%s/switch/state", device_identifier);
    snprintf(state_freq_topic, sizeof(state_freq_topic),
             "%s/sensor/freq", device_identifier);
    snprintf(state_duty_topic, sizeof(state_duty_topic),
             "%s/sensor/duty", device_identifier);

    snprintf(cmd_switch_topic, sizeof(cmd_switch_topic),
             "%s/switch/cmd", device_identifier);
    snprintf(cmd_freq_topic, sizeof(cmd_freq_topic),
             "%s/select/freq/cmd", device_identifier);
    snprintf(cmd_duty_topic, sizeof(cmd_duty_topic),
             "%s/select/duty/cmd", device_identifier);
}

// 发送Discovery配置
static void mqtt_send_discovery(void)
{
    if (mqtt_client == NULL || !mqtt_online) return;

    // 设备信息
    snprintf(device_info, sizeof(device_info),
             "{\"identifiers\":[\"%s\"],\"name\":\"ESP32 High Frequency\",\"manufacturer\":\"ESP32\",\"model\":\"C6\",\"sw_version\":\"1.0\"}",
             device_identifier);

    // 1. Switch Discovery (开关)
    snprintf(switch_payload, sizeof(switch_payload),
             "{\"name\":\"High Frequency Enable\",\"uniq_id\":\"%s_switch\","
             "\"dev\":%s,"
             "\"cmd_t\":\"%s\","
             "\"stat_t\":\"%s\","
             "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
             "\"icon\":\"mdi:pulse\"}",
             device_identifier, device_info, cmd_switch_topic, state_switch_topic);
    esp_mqtt_client_publish(mqtt_client, disc_switch_config, switch_payload, 0, 1, 0);

    // 2. Frequency Sensor Discovery (频率传感器)
    snprintf(freq_sensor_payload, sizeof(freq_sensor_payload),
             "{\"name\":\"Frequency\",\"uniq_id\":\"%s_freq\","
             "\"dev\":%s,"
             "\"stat_t\":\"%s\","
             "\"unit_of_meas\":\"Hz\","
             "\"icon\":\"mdi:sine-wave\","
             "\"val_tpl\":\"{{value|float(0)|int}}\"}",
             device_identifier, device_info, state_freq_topic);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_freq_config, freq_sensor_payload, 0, 1, 0);

    // 3. Duty Sensor Discovery (占空比传感器)
    snprintf(duty_sensor_payload, sizeof(duty_sensor_payload),
             "{\"name\":\"Duty Cycle\",\"uniq_id\":\"%s_duty\","
             "\"dev\":%s,"
             "\"stat_t\":\"%s\","
             "\"unit_of_meas\":\"%%\","
             "\"icon\":\"mdi:percent\","
             "\"val_tpl\":\"{{value|float(0)|int}}\"}",
             device_identifier, device_info, state_duty_topic);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_duty_config, duty_sensor_payload, 0, 1, 0);

    // 4. State Sensor Discovery (状态传感器)
    snprintf(state_sensor_payload, sizeof(state_sensor_payload),
             "{\"name\":\"Status\",\"uniq_id\":\"%s_state\","
             "\"dev\":%s,"
             "\"stat_t\":\"%s\","
             "\"icon\":\"mdi:check-circle\"}",
             device_identifier, device_info, state_switch_topic);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_state_config, state_sensor_payload, 0, 1, 0);

    // 5. Frequency Select Discovery (频率选择器)
    snprintf(freq_select_payload, sizeof(freq_select_payload),
             "{\"name\":\"Frequency Select\",\"uniq_id\":\"%s_freq_select\","
             "\"dev\":%s,"
             "\"cmd_t\":\"%s\","
             "\"stat_t\":\"%s\","
             "\"options\":["
             "\"20000\",\"30000\",\"40000\",\"50000\",\"60000\",\"70000\",\"80000\","
             "\"90000\",\"100000\",\"120000\",\"150000\",\"200000\",\"250000\",\"300000\"],"
             "\"icon\":\"mdi:sine-wave\","
             "\"retain\":true}",
             device_identifier, device_info, cmd_freq_topic, state_freq_topic);
    esp_mqtt_client_publish(mqtt_client, disc_select_freq_config, freq_select_payload, 0, 1, 0);

    // 6. Duty Select Discovery (占空比选择器)
    snprintf(duty_select_payload, sizeof(duty_select_payload),
             "{\"name\":\"Duty Cycle Select\",\"uniq_id\":\"%s_duty_select\","
             "\"dev\":%s,"
             "\"cmd_t\":\"%s\","
             "\"stat_t\":\"%s\","
             "\"options\":["
             "\"0\",\"10\",\"20\",\"30\",\"40\",\"50\",\"60\",\"70\",\"80\",\"90\",\"100\","
             "\"110\",\"120\",\"130\",\"140\",\"150\",\"160\",\"170\",\"180\",\"190\",\"200\","
             "\"210\",\"220\",\"230\",\"240\",\"250\"],"
             "\"icon\":\"mdi:percent\","
             "\"retain\":true}",
             device_identifier, device_info, cmd_duty_topic, state_duty_topic);
    esp_mqtt_client_publish(mqtt_client, disc_select_duty_config, duty_select_payload, 0, 1, 0);

    ESP_LOGI(TAG, "MQTT Discovery configs sent");
}

// 发布所有状态
static void mqtt_publish_all_states(void)
{
    if (mqtt_client == NULL || !mqtt_online) return;

    // 开关状态
    esp_mqtt_client_publish(mqtt_client, state_switch_topic,
                           pwm_switch ? "ON" : "OFF", 0, 1, 0);

    // 频率状态
    char freq_buf[32];
    snprintf(freq_buf, sizeof(freq_buf), "%lu", (unsigned long)pwm_freq);
    esp_mqtt_client_publish(mqtt_client, state_freq_topic, freq_buf, 0, 1, 0);

    // 占空比状态
    char duty_buf[32];
    snprintf(duty_buf, sizeof(duty_buf), "%d", pwm_duty);
    esp_mqtt_client_publish(mqtt_client, state_duty_topic, duty_buf, 0, 1, 0);
}

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

            get_device_identifier();

            // 订阅命令主题
            esp_mqtt_client_subscribe(evt->client, cmd_switch_topic, 0);
            esp_mqtt_client_subscribe(evt->client, cmd_freq_topic, 0);
            esp_mqtt_client_subscribe(evt->client, cmd_duty_topic, 0);

            // 发送Discovery配置
            mqtt_send_discovery();

            // 发布初始状态
            mqtt_publish_all_states();
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_online = false;
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)arg;
            if (client && !mqtt_online) {
                ESP_LOGE(TAG, "MQTT connection failed, likely configuration error");
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                wifi_force_reprovision();
            }
            break;

        case MQTT_EVENT_DATA:
        {
            ESP_LOGI(TAG, "MQTT data: %s = %s", evt->topic, evt->data);

            // 开关命令
            if (strcmp(evt->topic, cmd_switch_topic) == 0) {
                bool en = (strncmp(evt->data, "ON", 2) == 0);
                ledc_set_pwm_enable(en);
                nvs_save_all_param();
                esp_mqtt_client_publish(mqtt_client, state_switch_topic,
                                       en ? "ON" : "OFF", 0, 1, 0);
            }
            // 频率命令
            else if (strcmp(evt->topic, cmd_freq_topic) == 0) {
                uint32_t freq = atoi(evt->data);
                if (freq >= 10000 && freq <= 300000) {
                    ledc_set_pwm_freq(freq);
                    nvs_save_all_param();
                    char freq_buf[32];
                    snprintf(freq_buf, sizeof(freq_buf), "%lu", (unsigned long)freq);
                    esp_mqtt_client_publish(mqtt_client, state_freq_topic, freq_buf, 0, 1, 0);
                }
            }
            // 占空比命令
            else if (strcmp(evt->topic, cmd_duty_topic) == 0) {
                uint8_t duty = atoi(evt->data);
                if (duty <= 250) {
                    ledc_set_pwm_duty(duty);
                    nvs_save_all_param();
                    char duty_buf[32];
                    snprintf(duty_buf, sizeof(duty_buf), "%d", duty);
                    esp_mqtt_client_publish(mqtt_client, state_duty_topic, duty_buf, 0, 1, 0);
                }
            }
            break;
        }
    }
}

void mqtt_client_init(void)
{
    if (mqtt_uri[0] == '\0') {
        ESP_LOGW(TAG, "MQTT URI not configured, skipping MQTT initialization");
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
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        wifi_force_reprovision();
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT client started");
}

void mqtt_publish_device_status(void)
{
    mqtt_publish_all_states();
}
