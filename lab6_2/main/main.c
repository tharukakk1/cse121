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
#include "driver/temperature_sensor.h" // For onboard temp sensor 

static const char *TAG = "WEATHER_POST_CLIENT";

#define HOME_SSID       "WestmoorMesh"
#define HOME_PASS       "eastmoor"

// CRITICAL: Replace this with the actual IP address of your Raspberry Pi on your home network 
#define SERVER_IP       "192.168.4.50" 

void send_temperature_post(float current_temp) {
    char url_buf[64];
    // Format target destination to hit port 1234 precisely [cite: 571]
    snprintf(url_buf, sizeof(url_buf), "http://%s:1234/", SERVER_IP);

    char post_data_buf[64];
    // Format the payload string clearly [cite: 571]
    snprintf(post_data_buf, sizeof(post_data_buf), "Onboard Temp: %.1fC", current_temp);

    esp_http_client_config_t config = {
        .url = url_buf,
        .method = HTTP_METHOD_POST, // Set transaction to POST [cite: 571]
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Set headers and structural body payload [cite: 571]
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, post_data_buf, strlen(post_data_buf));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Success! Status Code = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST Request Failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void app_main(void) {
    // 1. Initialize NVS and Network Interfaces
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Initialize Internal Temperature Sensor 
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = {
        .range_min = 0,
        .range_max = 50,
    };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // 3. Connect to Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, HOME_SSID);
    strcpy((char *)wifi_config.sta.password, HOME_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait 5 seconds for connection stability and DHCP lease resolution [cite: 568]
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 4. Execution Loop
    while (1) {
        float sensor_temp = 0.0f;
        if (temperature_sensor_get_celsius(temp_sensor, &sensor_temp) == ESP_OK) { // 
            ESP_LOGI(TAG, "Reading Onboard Temp: %.1fC. Sending POST...", sensor_temp);
            send_temperature_post(sensor_temp); // [cite: 571]
        } else {
            ESP_LOGE(TAG, "Failed to read internal sensor.");
        }

        // Run periodically to avoid overwhelming the server loop
        vTaskDelay(pdMS_TO_TICKS(10000)); 
    }
}