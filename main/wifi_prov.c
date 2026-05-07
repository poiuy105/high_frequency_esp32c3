#include "wifi_prov.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_param.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WIFI_PROV";
static httpd_handle_t prov_server = NULL;
static bool prov_running = false;
static bool wifi_connected = false;

bool wifi_is_connected(void)
{
    return wifi_connected;
}

bool wifi_prov_is_running(void)
{
    return prov_running;
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi STA started");
        } else if (id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "WiFi connected to AP");
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_connected = false;
            ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
            esp_wifi_connect();
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

void wifi_start_sta(void)
{
    ESP_LOGI(TAG, "Initializing WiFi in STA mode");

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));

    // 配置WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, wifi_pswd, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "WiFi connecting to %s...", wifi_ssid);
}

// HTML页面 (嵌入式)
static const char index_html[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>ESP32-C6 配网</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}"
    ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
    "h2{text-align:center;color:#333}"
    "h3{color:#555;font-size:16px}"
    ".form-group{margin-bottom:15px}"
    "label{display:block;margin-bottom:5px;font-weight:bold;font-size:14px}"
    "input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px}"
    "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px}"
    "button:hover{background:#45a049}"
    ".section{margin-top:20px;padding-top:20px;border-top:1px solid #eee}"
    "#status{text-align:center;margin-top:15px;font-size:14px}"
    "</style></head><body>"
    "<div class=\"container\">"
    "<h2>ESP32-C6 配网</h2>"
    "<div class=\"section\">"
    "<h3>WiFi配置</h3>"
    "<div class=\"form-group\"><label>SSID (WiFi名称)</label>"
    "<input type=\"text\" id=\"ssid\" placeholder=\"输入WiFi名称\"></div>"
    "<div class=\"form-group\"><label>密码</label>"
    "<input type=\"password\" id=\"password\" placeholder=\"输入WiFi密码\"></div>"
    "</div>"
    "<div class=\"section\">"
    "<h3>MQTT配置</h3>"
    "<div class=\"form-group\"><label>Broker地址</label>"
    "<input type=\"text\" id=\"mqtt_uri\" placeholder=\"mqtt://192.168.1.100:1883\"></div>"
    "<div class=\"form-group\"><label>用户名 (可选)</label>"
    "<input type=\"text\" id=\"mqtt_user\" placeholder=\"MQTT用户名\"></div>"
    "<div class=\"form-group\"><label>密码 (可选)</label>"
    "<input type=\"password\" id=\"mqtt_pass\" placeholder=\"MQTT密码\"></div>"
    "</div>"
    "<button onclick=\"submitConfig()\">保存配置</button>"
    "<p id=\"status\"></p></div>"
    "<script>function submitConfig(){const config={ssid:document.getElementById('ssid').value,password:document.getElementById('password').value,mqtt_uri:document.getElementById('mqtt_uri').value,mqtt_user:document.getElementById('mqtt_user').value,mqtt_pass:document.getElementById('mqtt_pass').value};if(!config.ssid||!config.mqtt_uri){document.getElementById('status').textContent='请填写SSID和MQTT地址';return}document.getElementById('status').textContent='正在保存...';fetch('/provision',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(config)}).then(r=>r.json()).then(d=>{document.getElementById('status').textContent=d.status==='success'?'保存成功！设备正在重启...':'保存失败'}).catch(e=>{document.getElementById('status').textContent='网络错误'})}</script>"
    "</body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t provision_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    int cur_len = 0;
    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) break;
        cur_len += received;
    }
    buf[cur_len] = '\0';

    ESP_LOGI(TAG, "Received config: %s", buf);

    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
        cJSON *pass_json = cJSON_GetObjectItem(json, "password");
        cJSON *mqtt_uri_json = cJSON_GetObjectItem(json, "mqtt_uri");
        cJSON *mqtt_user_json = cJSON_GetObjectItem(json, "mqtt_user");
        cJSON *mqtt_pass_json = cJSON_GetObjectItem(json, "mqtt_pass");

        if (ssid_json && mqtt_uri_json) {
            strncpy(wifi_ssid, ssid_json->valuestring, sizeof(wifi_ssid) - 1);
            wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';

            if (pass_json && pass_json->valuestring) {
                strncpy(wifi_pswd, pass_json->valuestring, sizeof(wifi_pswd) - 1);
                wifi_pswd[sizeof(wifi_pswd) - 1] = '\0';
            }

            strncpy(mqtt_uri, mqtt_uri_json->valuestring, sizeof(mqtt_uri) - 1);
            mqtt_uri[sizeof(mqtt_uri) - 1] = '\0';

            if (mqtt_user_json && mqtt_user_json->valuestring) {
                strncpy(mqtt_user, mqtt_user_json->valuestring, sizeof(mqtt_user) - 1);
                mqtt_user[sizeof(mqtt_user) - 1] = '\0';
            }

            if (mqtt_pass_json && mqtt_pass_json->valuestring) {
                strncpy(mqtt_pass, mqtt_pass_json->valuestring, sizeof(mqtt_pass) - 1);
                mqtt_pass[sizeof(mqtt_pass) - 1] = '\0';
            }

            ESP_LOGI(TAG, "Saving config - SSID: %s, MQTT: %s", wifi_ssid, mqtt_uri);
            nvs_save_all_param();

            const char *resp = "{\"status\":\"success\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));

            ESP_LOGI(TAG, "Config saved, restarting in 1 second...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            const char *resp = "{\"status\":\"error\",\"message\":\"Missing required fields\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));
        }
        cJSON_Delete(json);
    } else {
        const char *resp = "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
    }

    free(buf);
    return ESP_OK;
}

void wifi_prov_start(void)
{
    if (prov_running) {
        ESP_LOGW(TAG, "Provisioning already running");
        return;
    }

    ESP_LOGI(TAG, "Starting AP provisioning...");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "===== AP 配网模式启动 =====");
    ESP_LOGI(TAG, "1. 连接WiFi热点: ESP32-C6_Setup");
    ESP_LOGI(TAG, "2. 热点密码: 12345678");
    ESP_LOGI(TAG, "3. 浏览器打开: http://192.168.4.1");
    ESP_LOGI(TAG, "4. 填写WiFi和MQTT配置");
    ESP_LOGI(TAG, "========================================");

    // 初始化网络接口（如果还没初始化）
    static bool netif_init = false;
    if (!netif_init) {
        esp_netif_init();
        esp_event_loop_create_default();
        netif_init = true;
    }

    // 创建AP网络接口
    esp_netif_create_default_wifi_ap();

    // 初始化WiFi（如果还没初始化）
    static bool wifi_init = false;
    if (!wifi_init) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        wifi_init = true;
    }

    // 配置SoftAP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32-C6_Setup",
            .ssid_len = strlen("ESP32-C6_Setup"),
            .channel = 1,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    ESP_LOGI(TAG, "SoftAP started: SSID=ESP32-C6_Setup, PASSWORD=12345678");

    // 启动HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    if (httpd_start(&prov_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(prov_server, &index_uri);

        httpd_uri_t prov_uri = {
            .uri = "/provision",
            .method = HTTP_POST,
            .handler = provision_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(prov_server, &prov_uri);

        ESP_LOGI(TAG, "HTTP server started, connect to http://192.168.4.1");
        prov_running = true;
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
