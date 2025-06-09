#include <bsp/m5stack_tab5.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <string.h>

#define WIFI_SSID "tab5minimal"

static const char *TAG = "app";

static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, WIFI_SSID);
    ap_config.ap.ssid_len = strlen(WIFI_SSID);
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 4;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started SSID:%s", WIFI_SSID);
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    bsp_i2c_init();
    lv_display_t *disp = bsp_display_start();
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    bsp_display_backlight_on();

    wifi_init_ap();

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, WIFI_SSID);
    lv_obj_center(label);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
