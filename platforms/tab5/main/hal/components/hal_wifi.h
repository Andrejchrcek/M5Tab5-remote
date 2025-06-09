#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <esp_http_server.h>

const char* hal_wifi_get_ap_ssid();
void wifi_init_apsta();
httpd_handle_t start_webserver();

#ifdef __cplusplus
}
#endif
