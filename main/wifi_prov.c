#include "wifi_prov.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_param.h"
#include "cJSON.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WIFI_PROV";
static httpd_handle_t prov_server = NULL;
static bool prov_running = false;
static bool wifi_connected = false;
static bool force_reprovision = false;

// DNS 服务器任务句柄
static TaskHandle_t dns_task_handle = NULL;

// DNS 服务器任务 - 劫持所有DNS请求并返回ESP32的IP（前向声明）
static void dns_server_task(void *pvParameters);

// 获取MAC地址后缀
static void get_mac_suffix(char *buf, size_t len)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

bool wifi_is_connected(void)
{
    return wifi_connected;
}

bool wifi_prov_is_running(void)
{
    return prov_running;
}

// 强制重新配网（MQTT连接失败时调用）
void wifi_force_reprovision(void)
{
    ESP_LOGW(TAG, "MQTT configuration invalid, forcing re-provisioning");
    // 清除WiFi配置，下次启动会进入配网模式
    wifi_ssid[0] = '\0';
    wifi_pswd[0] = '\0';
    nvs_save_all_param();
    ESP_LOGW(TAG, "WiFi config cleared, rebooting to provisioning mode...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
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
    "<title>ESP32 配网</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}"
    ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
    "h2{text-align:center;color:#333}"
    "h3{color:#555;font-size:16px}"
    ".form-group{margin-bottom:15px}"
    "label{display:block;margin-bottom:5px;font-weight:bold;font-size:14px}"
    "input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px}"
    ".input-row{display:flex;gap:10px}"
    ".input-row input:first-child{flex:2}"
    ".input-row input:last-child{flex:1}"
    "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px}"
    "button:hover{background:#45a049}"
    ".section{margin-top:20px;padding-top:20px;border-top:1px solid #eee}"
    "#status{text-align:center;margin-top:15px;font-size:14px}"
    ".error{color:red}"
    ".success{color:green}"
    "</style></head><body>"
    "<div class=\"container\">"
    "<h2>ESP32 高频发生器</h2>"
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
    "<input type=\"text\" id=\"mqtt_host\" placeholder=\"192.168.1.100\"></div>"
    "<div class=\"form-group\"><label>端口</label>"
    "<input type=\"number\" id=\"mqtt_port\" placeholder=\"1883\" value=\"1883\"></div>"
    "<div class=\"form-group\"><label>用户名 (可选)</label>"
    "<input type=\"text\" id=\"mqtt_user\" placeholder=\"MQTT用户名\"></div>"
    "<div class=\"form-group\"><label>密码 (可选)</label>"
    "<input type=\"password\" id=\"mqtt_pass\" placeholder=\"MQTT密码\"></div>"
    "</div>"
    "<button onclick=\"submitConfig()\">保存配置</button>"
    "<p id=\"status\"></p></div>"
    "<script>function submitConfig(){const ssid=document.getElementById('ssid').value;const password=document.getElementById('password').value;const mqttHost=document.getElementById('mqtt_host').value;const mqttPort=document.getElementById('mqtt_port').value||'1883';const mqttUser=document.getElementById('mqtt_user').value;const mqttPass=document.getElementById('mqtt_pass').value;if(!ssid||!mqttHost){document.getElementById('status').className='error';document.getElementById('status').textContent='请填写SSID和MQTT地址';return}document.getElementById('status').className='';document.getElementById('status').textContent='正在保存...';const mqttUri='mqtt://'+mqttHost+':'+mqttPort;const config={ssid:ssid,password:password,mqtt_uri:mqttUri,mqtt_user:mqttUser,mqtt_pass:mqttPass};fetch('/provision',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(config)}).then(r=>r.json()).then(d=>{if(d.status==='success'){document.getElementById('status').className='success';document.getElementById('status').textContent='保存成功！设备正在重启...'}else{document.getElementById('status').className='error';document.getElementById('status').textContent='保存失败: '+(d.message||'未知错误')}}).catch(e=>{document.getElementById('status').className='error';document.getElementById('status').textContent='网络错误'})}</script>"
    "</body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

// Captive Portal 检测处理 - 返回204 No Content
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    // Android/Captive Portal 检测需要返回 204 No Content
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "Captive portal detection handled");
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
            const char *resp = "{\"status\":\"error\",\"message\":\"缺少必填字段\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));
        }
        cJSON_Delete(json);
    } else {
        const char *resp = "{\"status\":\"error\",\"message\":\"JSON格式错误\"}";
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

    // 获取MAC后缀
    char mac_suffix[16];
    get_mac_suffix(mac_suffix, sizeof(mac_suffix));

    char ap_ssid[64];
    snprintf(ap_ssid, sizeof(ap_ssid), "ESP32 High Frequency_%s", mac_suffix);

    ESP_LOGI(TAG, "Starting AP provisioning...");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "===== AP 配网模式启动 =====");
    ESP_LOGI(TAG, "1. 连接WiFi热点: %s", ap_ssid);
    ESP_LOGI(TAG, "2. 热点无密码，直接连接");
    ESP_LOGI(TAG, "3. 浏览器会自动弹出配置页面");
    ESP_LOGI(TAG, "4. 或手动打开: http://192.168.4.1");
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

    // 配置SoftAP - 无密码
    wifi_config_t ap_config = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        }
    };
    strncpy((char*)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(ap_ssid);

    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    ESP_LOGI(TAG, "SoftAP started: SSID=%s (no password)", ap_ssid);

    // 配置DHCP服务器，设置DNS为ESP32自己的IP
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif) {
        // 停止DHCP服务器
        esp_netif_dhcps_stop(ap_netif);
        
        // 配置IP地址
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_set_ip_info(ap_netif, &ip_info);
        
        // 配置DHCP选项：DNS服务器指向自己
        uint8_t dhcp_option_6[4] = {192, 168, 4, 1}; // DNS服务器地址
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, dhcp_option_6, sizeof(dhcp_option_6));
        
        // 重新启动DHCP服务器
        esp_netif_dhcps_start(ap_netif);
        
        ESP_LOGI(TAG, "DHCP server configured with DNS: 192.168.4.1");
    }

    // 启动DNS劫持服务器
    if (dns_task_handle == NULL) {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
        ESP_LOGI(TAG, "DNS hijacking server started");
    }

    // 启动HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    if (httpd_start(&prov_server, &config) == ESP_OK) {
        // 注册主页处理
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(prov_server, &index_uri);

        // 注册provision处理
        httpd_uri_t prov_uri = {
            .uri = "/provision",
            .method = HTTP_POST,
            .handler = provision_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(prov_server, &prov_uri);

        // 注册 Captive Portal 检测 URL - 只保留最关键的两个
        const char* captive_urls[] = {
            "/generate_204",    // Android
            "/gen_204"          // Android alternative
        };
        
        for (int i = 0; i < sizeof(captive_urls)/sizeof(captive_urls[0]); i++) {
            httpd_uri_t captive_uri = {
                .uri = captive_urls[i],
                .method = HTTP_GET,
                .handler = captive_portal_handler,
                .user_ctx = NULL
            };
            esp_err_t ret = httpd_register_uri_handler(prov_server, &captive_uri);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Registered captive URL: %s", captive_urls[i]);
            } else {
                ESP_LOGW(TAG, "Failed to register %s (err=%d)", captive_urls[i], ret);
            }
        }

        // 注册通配符处理 - 用于其他所有请求重定向到主页
        httpd_uri_t catch_all_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(prov_server, &catch_all_uri);

        ESP_LOGI(TAG, "HTTP server started, connect to http://192.168.4.1");
        prov_running = true;
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

// DNS 服务器任务 - 劫持所有DNS请求并返回ESP32的IP
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[512];  // 增大缓冲区以支持更长的DNS查询
    struct sockaddr_in6 source_addr;
    socklen_t socklen = sizeof(source_addr);

    // 创建持久化的UDP socket
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS server socket created");

    // 允许地址重用
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        closesocket(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS server bound to port 53");

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // 主循环 - 持续处理DNS请求
    while (1) {
        // 接收DNS请求
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            // 获取客户端IP地址用于日志
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)&source_addr)->sin_addr, client_ip, sizeof(client_ip));
            ESP_LOGI(TAG, "DNS request from %s (len=%d)", client_ip, len);
            
            // 构造DNS响应 - 将所有域名解析为 192.168.4.1
            char response[512];
            memcpy(response, rx_buffer, len); // 复制请求头
            
            // 设置QR位为1（响应）
            response[2] |= 0x80;
            // 设置RA位为1
            response[3] |= 0x80;
            // 设置答案数量为1
            response[6] = 0x00;
            response[7] = 0x01;
            
            // 添加答案记录
            int resp_len = len;
            
            // 添加指向问题的指针（使用压缩指针）
            response[resp_len++] = 0xC0;  // 指针标记
            response[resp_len++] = 0x0C;  // 指向第12字节（问题部分）
            
            // 类型：A记录 (1)
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x01;
            
            // 类：IN (1)
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x01;
            
            // TTL: 60秒
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x3C;
            
            // 数据长度：4字节（IPv4地址）
            response[resp_len++] = 0x00;
            response[resp_len++] = 0x04;
            
            // IP地址：192.168.4.1
            response[resp_len++] = 192;
            response[resp_len++] = 168;
            response[resp_len++] = 4;
            response[resp_len++] = 1;
            
            // 发送响应
            int sent = sendto(sock, response, resp_len, 0, (struct sockaddr *)&source_addr, socklen);
            if (sent > 0) {
                ESP_LOGI(TAG, "DNS response sent: all domains -> 192.168.4.1 (%d bytes)", sent);
            } else {
                ESP_LOGW(TAG, "Failed to send DNS response: errno %d", errno);
            }
        } else if (len == 0) {
            ESP_LOGW(TAG, "DNS connection closed");
        } else {
            // 超时或其他错误，继续循环
            ESP_LOGD(TAG, "DNS recv timeout or error: errno %d", errno);
        }
    }

    // 正常情况下不会到达这里
    closesocket(sock);
    ESP_LOGW(TAG, "DNS server task exiting");
    vTaskDelete(NULL);
}
