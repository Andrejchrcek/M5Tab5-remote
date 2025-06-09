#include "hal/hal_esp32.h"
#include "hal/components/hal_wifi.h"
#include <bsp/m5stack_tab5.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" void app_main(void)
{
    HalEsp32 hal;
    hal.startWifiAp();

    lv_display_t* disp = bsp_display_start();
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
