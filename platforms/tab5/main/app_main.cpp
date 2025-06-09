#include "hal/components/hal_wifi.h"
#include <bsp/m5stack_tab5.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    wifi_init_apsta();
    start_webserver();

    bsp_display_start();
    bsp_display_backlight_on();

    const char* ssid = hal_wifi_get_ap_ssid();
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, ssid);
    lv_obj_center(label);

    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
