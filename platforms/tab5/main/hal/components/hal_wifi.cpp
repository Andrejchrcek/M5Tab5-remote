/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <mooncake_log.h>
#include <vector>
#include <memory>
#include <string.h>
#include <bsp/m5stack_tab5.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_http_server.h>

#define TAG "wifi"

#define WIFI_SSID    "M5Tab5-UserDemo-WiFi"
#define WIFI_PASS    ""
#define MAX_STA_CONN 4

static char g_ap_ssid[32]  = WIFI_SSID;
static char g_sta_ssid[32] = "";
static char g_sta_pass[64] = "";
static bool g_wifi_started = false;

static void url_decode(char* dst, const char* src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && isxdigit(a) && isxdigit(b)) {
            a = (a >= 'a') ? a - 'a' + 10 : (a >= 'A') ? a - 'A' + 10 : a - '0';
            b = (b >= 'a') ? b - 'a' + 10 : (b >= 'A') ? b - 'A' + 10 : b - '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static void wifi_reconfigure()
{
    if (g_wifi_started) {
        esp_wifi_stop();
    }

    wifi_config_t ap_config = {};
    strncpy(reinterpret_cast<char*>(ap_config.ap.ssid), g_ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len       = strlen(g_ap_ssid);
    ap_config.ap.max_connection = MAX_STA_CONN;
    ap_config.ap.authmode       = WIFI_AUTH_OPEN;

    wifi_config_t sta_config = {};
    if (strlen(g_sta_ssid) > 0) {
        strncpy(reinterpret_cast<char*>(sta_config.sta.ssid), g_sta_ssid, sizeof(sta_config.sta.ssid));
        strncpy(reinterpret_cast<char*>(sta_config.sta.password), g_sta_pass, sizeof(sta_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    if (strlen(g_sta_ssid) > 0) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    g_wifi_started = true;
    if (strlen(g_sta_ssid) > 0) {
        esp_wifi_connect();
    }
}

// HTTP handler: configuration page
esp_err_t config_get_handler(httpd_req_t* req)
{
    // The HTML page contains several input fields for SSIDs and password.
    // With maximum lengths of the credentials the generated page can exceed
    // 512 bytes, which triggered a -Wformat-truncation build error.
    // Use a larger buffer to hold the entire page comfortably.
    char html[768];
    snprintf(html, sizeof(html),
             R"rawliteral(
<!DOCTYPE html>
<html>
  <head><title>M5Tab5 WiFi</title></head>
  <body>
    <h2>WiFi Settings</h2>
    <form action='/config' method='post'>
      <h3>Access Point</h3>
      SSID:<input name='ap_ssid' value='%s'><br>
      <h3>Station</h3>
      SSID:<input name='sta_ssid' value='%s'><br>
      Password:<input type='password' name='sta_pass' value='%s'><br>
      <input type='submit' value='Save'>
    </form>
  </body>
</html>
)rawliteral",
             g_ap_ssid, g_sta_ssid, g_sta_pass);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

// HTTP handler: receive configuration
esp_err_t config_post_handler(httpd_req_t* req)
{
    char buf[256];
    int received = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (received <= 0) {
        return ESP_FAIL;
    }
    buf[received] = '\0';

    char *tok = strtok(buf, "&");
    while (tok) {
        if (strncmp(tok, "ap_ssid=", 8) == 0) {
            url_decode(g_ap_ssid, tok + 8);
        } else if (strncmp(tok, "sta_ssid=", 9) == 0) {
            url_decode(g_sta_ssid, tok + 9);
        } else if (strncmp(tok, "sta_pass=", 9) == 0) {
            url_decode(g_sta_pass, tok + 9);
        }
        tok = strtok(NULL, "&");
    }

    wifi_reconfigure();

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// URI routes
httpd_uri_t root_uri   = {.uri = "/", .method = HTTP_GET, .handler = config_get_handler, .user_ctx = nullptr};
httpd_uri_t post_uri   = {.uri = "/config", .method = HTTP_POST, .handler = config_post_handler, .user_ctx = nullptr};

// 启动 Web Server
httpd_handle_t start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = nullptr;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &post_uri);
    }
    return server;
}

// Initialize Wi-Fi in AP+STA mode
void wifi_init_apsta()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_reconfigure();
}

static void wifi_ap_test_task(void* param)
{
    wifi_init_apsta();
    start_webserver();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

bool HalEsp32::wifi_init()
{
    mclog::tagInfo(TAG, "wifi init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(wifi_ap_test_task, "ap", 4096, nullptr, 5, nullptr);
    return true;
}

void HalEsp32::setExtAntennaEnable(bool enable)
{
    _ext_antenna_enable = enable;
    mclog::tagInfo(TAG, "set ext antenna enable: {}", _ext_antenna_enable);
    bsp_set_ext_antenna_enable(_ext_antenna_enable);
}

bool HalEsp32::getExtAntennaEnable()
{
    return _ext_antenna_enable;
}

void HalEsp32::startWifiAp()
{
    wifi_init();
}
