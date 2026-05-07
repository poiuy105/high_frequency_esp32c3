#include "wifi_airkiss.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_param.h"
#include <string.h>

bool wifi_connected = false;

static bool smartconfig_running = false;

bool wifi_is_connected(void)
{
    return wifi_connected;
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI("WIFI", "WiFi STA started");
        // 禁用WiFi省电模式，确保SmartConfig正常工作
        esp_wifi_set_ps(WIFI_PS_NONE);
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI("WIFI", "WiFi connected to AP");
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGW("WIFI", "WiFi disconnected");
        // 如果SmartConfig正在运行，不要自动重连
        if (!smartconfig_running) {
            ESP_LOGI("WIFI", "Reconnecting...");
            esp_wifi_connect();
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
        ESP_LOGI("WIFI", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

static void smartconfig_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    switch (id) {
        case SC_EVENT_SCAN_DONE:
            ESP_LOGI("SC", "Channel scan done");
            ESP_LOGI("SC", "Waiting for SmartConfig packet from phone...");
            ESP_LOGI("SC", "Make sure your phone is connected to 2.4GHz WiFi");
            break;

        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGI("SC", "Found SmartConfig channel!");
            ESP_LOGI("SC", "Receiving configuration data...");
            break;

        case SC_EVENT_GOT_SSID_PSWD:
            ESP_LOGI("SC", "Received SSID and password!");
            smartconfig_event_got_ssid_pswd_t* evt = (smartconfig_event_got_ssid_pswd_t*) data;
            ESP_LOGI("SC", "SSID: %s", evt->ssid);

            // 保存WiFi凭据到NVS
            strncpy(wifi_ssid, (char*)evt->ssid, sizeof(wifi_ssid) - 1);
            strncpy(wifi_pswd, (char*)evt->password, sizeof(wifi_pswd) - 1);
            nvs_save_all_param();

            // 停止SmartConfig
            smartconfig_running = false;
            esp_smartconfig_stop();

            // 配置并连接WiFi
            wifi_config_t wifi_config = {0};
            memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            break;

        case SC_EVENT_SEND_ACK_DONE:
            ESP_LOGI("SC", "ACK sent successfully");
            smartconfig_running = false;
            break;
    }
}

void wifi_airkiss_start(void)
{
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 设置国家码（中国），启用全部13个信道
    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));

    // 设置WiFi功率为最大，确保SmartConfig能接收到数据
    esp_wifi_set_max_tx_power(84); // 20.5 dBm

    // 注册WiFi事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // 注册IP事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));

    // 注册SmartConfig事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL, NULL));

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "WiFi started in STA mode");

    // 如果有保存的SSID，尝试连接
    if (wifi_ssid[0] != '\0') {
        ESP_LOGI("WIFI", "Found saved SSID: %s", wifi_ssid);
        wifi_config_t wifi_config = {0};
        strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, wifi_pswd, sizeof(wifi_config.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else {
        // 没有保存的SSID，启动SmartConfig
        ESP_LOGI("WIFI", "No saved WiFi credentials");
        ESP_LOGI("WIFI", "Starting SmartConfig (AirKiss) mode...");
        ESP_LOGI("WIFI", "");
        ESP_LOGI("WIFI", "===== 配网说明 =====");
        ESP_LOGI("WIFI", "1. 确保手机连接到2.4GHz WiFi");
        ESP_LOGI("WIFI", "2. 下载'ESP SmartConfig'或'Esptouch' App");
        ESP_LOGI("WIFI", "3. 输入WiFi密码并点击配置");
        ESP_LOGI("WIFI", "4. 手机靠近设备(<2米)");
        ESP_LOGI("WIFI", "==================");
        ESP_LOGI("WIFI", "");

        // 设置SmartConfig类型为AirKiss
        ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_AIRKISS));

        // 配置SmartConfig参数
        smartconfig_start_config_t sc_cfg = SMARTCONFIG_START_CONFIG_DEFAULT();

        smartconfig_running = true;
        ESP_ERROR_CHECK(esp_smartconfig_start(&sc_cfg));

        ESP_LOGI("SC", "SmartConfig is now running!");
        ESP_LOGI("SC", "Device is listening for configuration packets...");
    }
}
