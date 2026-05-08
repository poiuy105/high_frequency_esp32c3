#include "mqtt_ha_harmony.h"
#include "mqtt_client.h"
#include "nvs_param.h"
#include "ledc_pwm.h"
#include "wifi_prov.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

bool mqtt_online = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "MQTT";

// 设备唯一标识符
static char device_identifier[24] = {0};

// Discovery topics
static char disc_switch_config[128];
static char disc_number_freq_config[128];
static char disc_number_duty_config[128];
static char disc_sensor_uptime_config[128];
static char disc_sensor_wifi_config[128];
static char disc_sensor_heap_config[128];
static char disc_sensor_ip_config[128];

// State topics
static char state_switch_topic[64];
static char state_freq_topic[64];
static char state_duty_topic[64];
static char state_uptime_topic[64];
static char state_wifi_topic[64];
static char state_heap_topic[64];
static char state_ip_topic[64];

// Command topics
static char cmd_switch_topic[64];
static char cmd_freq_topic[64];
static char cmd_duty_topic[64];

// Payload buffers
static char device_info[512];
static char switch_payload[1024];
static char freq_number_payload[1024];
static char duty_number_payload[1024];
static char uptime_sensor_payload[1024];
static char wifi_sensor_payload[1024];
static char heap_sensor_payload[1024];
static char ip_sensor_payload[1024];

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
    snprintf(disc_number_freq_config, sizeof(disc_number_freq_config),
             "homeassistant/number/%s_freq/config", device_identifier);
    snprintf(disc_number_duty_config, sizeof(disc_number_duty_config),
             "homeassistant/number/%s_duty/config", device_identifier);
    snprintf(disc_sensor_uptime_config, sizeof(disc_sensor_uptime_config),
             "homeassistant/sensor/%s_uptime/config", device_identifier);
    snprintf(disc_sensor_wifi_config, sizeof(disc_sensor_wifi_config),
             "homeassistant/sensor/%s_wifi/config", device_identifier);
    snprintf(disc_sensor_heap_config, sizeof(disc_sensor_heap_config),
             "homeassistant/sensor/%s_heap/config", device_identifier);
    snprintf(disc_sensor_ip_config, sizeof(disc_sensor_ip_config),
             "homeassistant/sensor/%s_ip/config", device_identifier);

    snprintf(state_switch_topic, sizeof(state_switch_topic),
             "%s/switch/state", device_identifier);
    snprintf(state_freq_topic, sizeof(state_freq_topic),
             "%s/number/freq", device_identifier);
    snprintf(state_duty_topic, sizeof(state_duty_topic),
             "%s/number/duty", device_identifier);
    snprintf(state_uptime_topic, sizeof(state_uptime_topic),
             "%s/sensor/uptime", device_identifier);
    snprintf(state_wifi_topic, sizeof(state_wifi_topic),
             "%s/sensor/wifi", device_identifier);
    snprintf(state_heap_topic, sizeof(state_heap_topic),
             "%s/sensor/heap", device_identifier);
    snprintf(state_ip_topic, sizeof(state_ip_topic),
             "%s/sensor/ip", device_identifier);

    snprintf(cmd_switch_topic, sizeof(cmd_switch_topic),
             "%s/switch/cmd", device_identifier);
    snprintf(cmd_freq_topic, sizeof(cmd_freq_topic),
             "%s/number/freq/cmd", device_identifier);
    snprintf(cmd_duty_topic, sizeof(cmd_duty_topic),
             "%s/number/duty/cmd", device_identifier);
}

// 发送Discovery配置
static void mqtt_send_discovery(void)
{
    if (mqtt_client == NULL || !mqtt_online) return;

    // 设备信息
    snprintf(device_info, sizeof(device_info),
             "{\"identifiers\":[\"%s\"],\"name\":\"ESP32 High Frequency\",\"manufacturer\":\"ESP32\",\"model\":\"C6\",\"sw_version\":\"1.0\"}",
             device_identifier);

    // 1. Switch Discovery (PWM开关)
    snprintf(switch_payload, sizeof(switch_payload),
             "{\"name\":\"PWM Enable\",\"uniq_id\":\"%s_switch\","
             "\"dev\":%s,"
             "\"cmd_t\":\"~/%s\","
             "\"stat_t\":\"~/%s\","
             "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
             "\"icon\":\"mdi:pulse\","
             "\"~\":\"%s\"}",
             device_identifier, device_info, "switch/cmd", "switch/state", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_switch_config, switch_payload, 0, 1, 0);

    // 2. Frequency Number Discovery (频率控制)
    snprintf(freq_number_payload, sizeof(freq_number_payload),
             "{\"name\":\"Frequency\",\"uniq_id\":\"%s_freq\","
             "\"dev\":%s,"
             "\"cmd_t\":\"~/%s\","
             "\"stat_t\":\"~/%s\","
             "\"min\":10000,\"max\":300000,\"step\":1000,"
             "\"unit_of_meas\":\"Hz\","
             "\"icon\":\"mdi:sine-wave\","
             "\"mode\":\"box\","
             "\"~\":\"%s\"}",
             device_identifier, device_info, "number/freq/cmd", "number/freq", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_number_freq_config, freq_number_payload, 0, 1, 0);

    // 3. Duty Number Discovery (占空比控制)
    snprintf(duty_number_payload, sizeof(duty_number_payload),
             "{\"name\":\"Duty Cycle\",\"uniq_id\":\"%s_duty\","
             "\"dev\":%s,"
             "\"cmd_t\":\"~/%s\","
             "\"stat_t\":\"~/%s\","
             "\"min\":0,\"max\":250,\"step\":1,"
             "\"unit_of_meas\":\"%%\","
             "\"icon\":\"mdi:percent\","
             "\"mode\":\"box\","
             "\"~\":\"%s\"}",
             device_identifier, device_info, "number/duty/cmd", "number/duty", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_number_duty_config, duty_number_payload, 0, 1, 0);

    // 4. Uptime Sensor Discovery (运行时间)
    snprintf(uptime_sensor_payload, sizeof(uptime_sensor_payload),
             "{\"name\":\"Uptime\",\"uniq_id\":\"%s_uptime\","
             "\"dev\":%s,"
             "\"stat_t\":\"~/%s\","
             "\"unit_of_meas\":\"s\","
             "\"icon\":\"mdi:clock\","
             "\"entity_category\":\"diagnostic\","
             "\"~\":\"%s\"}",
             device_identifier, device_info, "sensor/uptime", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_uptime_config, uptime_sensor_payload, 0, 1, 0);

    // 5. WiFi Sensor Discovery (WiFi信号强度)
    snprintf(wifi_sensor_payload, sizeof(wifi_sensor_payload),
             "{\"name\":\"WiFi Signal\",\"uniq_id\":\"%s_wifi\","
             "\"dev\":%s,"
             "\"stat_t\":\"~/%s\","
             "\"unit_of_meas\":\"dBm\","
             "\"icon\":\"mdi:wifi\","
             "\"entity_category\":\"diagnostic\","
             "\"~\":\"%s\"}",
             device_identifier, device_info, "sensor/wifi", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_wifi_config, wifi_sensor_payload, 0, 1, 0);

    // 6. Heap Free Sensor Discovery (空闲内存)
    snprintf(heap_sensor_payload, sizeof(heap_sensor_payload),
             "{\"name\":\"Free Memory\",\"uniq_id\":\"%s_heap\"," 
             "\"dev\":%s," 
             "\"stat_t\":\"~/%s\"," 
             "\"unit_of_meas\":\"bytes\"," 
             "\"icon\":\"mdi:memory\"," 
             "\"entity_category\":\"diagnostic\"," 
             "\"state_class\":\"measurement\"," 
             "\"~\":\"%s\"}",
             device_identifier, device_info, "sensor/heap", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_heap_config, heap_sensor_payload, 0, 1, 0);
    
    // 7. IP Address Sensor Discovery (IP地址)
    snprintf(ip_sensor_payload, sizeof(ip_sensor_payload),
             "{\"name\":\"IP Address\",\"uniq_id\":\"%s_ip\"," 
             "\"dev\":%s," 
             "\"stat_t\":\"~/%s\"," 
             "\"icon\":\"mdi:ip-network\"," 
             "\"entity_category\":\"diagnostic\"," 
             "\"~\":\"%s\"}",
             device_identifier, device_info, "sensor/ip", device_identifier);
    esp_mqtt_client_publish(mqtt_client, disc_sensor_ip_config, ip_sensor_payload, 0, 1, 0);

    ESP_LOGI(TAG, "MQTT Discovery configs sent");
}

// 获取WiFi RSSI
static int get_wifi_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        return ap_info.rssi;
    }
    return -127;
}

// 获取IP地址
static void get_ip_address(char *buf, size_t len)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(buf, len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(buf, len, "0.0.0.0");
    }
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

    // 运行时间 (秒)
    char uptime_buf[32];
    snprintf(uptime_buf, sizeof(uptime_buf), "%" PRId64, esp_timer_get_time() / 1000000);
    esp_mqtt_client_publish(mqtt_client, state_uptime_topic, uptime_buf, 0, 1, 0);

    // WiFi信号强度
    char wifi_buf[32];
    snprintf(wifi_buf, sizeof(wifi_buf), "%d", get_wifi_rssi());
    esp_mqtt_client_publish(mqtt_client, state_wifi_topic, wifi_buf, 0, 1, 0);

    // 空闲内存
    char heap_buf[32];
    snprintf(heap_buf, sizeof(heap_buf), "%lu", (unsigned long)esp_get_free_heap_size());
    esp_mqtt_client_publish(mqtt_client, state_heap_topic, heap_buf, 0, 1, 0);

    // IP地址
    char ip_buf[32];
    get_ip_address(ip_buf, sizeof(ip_buf));
    esp_mqtt_client_publish(mqtt_client, state_ip_topic, ip_buf, 0, 1, 0);
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
