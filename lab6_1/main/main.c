#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"

static const char *TAG = "WEATHER_STATION";

// Network Configurations
#define HOME_SSID       "WestmoorMesh"
#define HOME_PASS       "eastmoor"

#define LAB_SSID        "JBE-301A"             // Target Lab Network Name
#define LAB_PASS        "LAB_PASSWORD_HERE"    // Replace with Lab Password from Canvas

// Change this flag to true when you physically relocate to the lab
#define USE_LAB_WIFI    false 

// HTTP Event Handler to process the incoming wttr.in stream data
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Print the raw string snippet payload returned from wttr.in
            if (evt->data_len > 0) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Function to perform the HTTP GET Request
void get_weather_data(void) {
    esp_http_client_config_t config = {
        // format=1 returns JUST the raw temperature (e.g., +20°C)
        // &m explicitly forces the output into metric system (Celsius)
        .url = "http://wttr.in/95064?format=1&m", 
        .event_handler = http_event_handler,
    };
    
    // Print the static prefix label right before the raw server data loads
    printf("UC Santa Cruz 95064: "); 
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        printf("\n"); // Clear line formatting after the raw data rolls in
        ESP_LOGI(TAG, "HTTP GET Status = %d", esp_http_client_get_status_code(client));
    } else {
        printf("\n");
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void app_main(void) {
    // 1. Initialize NVS (Required for internal Wi-Fi stack calibration tracking)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize Core Network TCP/IP Interfaces
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Configure Wi-Fi Driver Drivers Layout Settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    if (USE_LAB_WIFI) {
        strcpy((char *)wifi_config.sta.ssid, LAB_SSID);
        strcpy((char *)wifi_config.sta.password, LAB_PASS);
    } else {
        strcpy((char *)wifi_config.sta.ssid, HOME_SSID);
        strcpy((char *)wifi_config.sta.password, HOME_PASS);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Allow a few seconds to safely settle DHCP IP acquisition lease handshake
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    // 4. Hit the Endpoint to print out outdoor data
    ESP_LOGI(TAG, "Querying weather endpoint...");
    get_weather_data();
}