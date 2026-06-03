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
#include "driver/temperature_sensor.h"

static const char *TAG = "INTEGRATED_WEATHER";

#define HOME_SSID       "WestmoorMesh"
#define HOME_PASS       "eastmoor"
#define SERVER_IP       "192.168.4.50" // Replace with your Pi's actual local IP address

// Global buffers to share data across network event cycles safely
char location_buffer[32] = {0};
char outdoor_temp_buffer[16] = {0};

// Event Handler to grab the Server Location (GET /location)
esp_err_t location_http_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy_len = (evt->data_len < sizeof(location_buffer) - 1) ? evt->data_len : sizeof(location_buffer) - 1;
        memcpy(location_buffer, evt->data, copy_len);
        location_buffer[copy_len] = '\0'; // Enforce null termination
    }
    return ESP_OK;
}

// Event Handler to grab the Outdoor Weather string from wttr.in
esp_err_t outdoor_http_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy_len = (evt->data_len < sizeof(outdoor_temp_buffer) - 1) ? evt->data_len : sizeof(outdoor_temp_buffer) - 1;
        memcpy(outdoor_temp_buffer, evt->data, copy_len);
        outdoor_temp_buffer[copy_len] = '\0';
    }
    return ESP_OK;
}

// 1. STEP A: Get Server Location
void fetch_server_location(void) {
    char url_buf[64];
    snprintf(url_buf, sizeof(url_buf), "http://%s:1234/location", SERVER_IP);
    memset(location_buffer, 0, sizeof(location_buffer));

    esp_http_client_config_t config = {
        .url = url_buf,
        .event_handler = location_http_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// 2. STEP B: Get Outdoor Weather based on the retrieved location
// void fetch_outdoor_weather(void) {
//     char url_buf[128];
//     // Strip trailing spaces or formatting issues if any, target format=1&m for Celsius
//     snprintf(url_buf, sizeof(url_buf), "http://wttr.in/%s?format=1&m", location_buffer);
//     memset(outdoor_temp_buffer, 0, sizeof(outdoor_temp_buffer));

//     esp_http_client_config_t config = {
//         .url = url_buf,
//         .event_handler = outdoor_http_handler,
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     esp_http_client_perform(client);
//     esp_http_client_cleanup(client);
    
//     // Clean up any trailing newline symbols coming back from wttr.in
//     strtok(outdoor_temp_buffer, "\n"); 
// }
// 2. STEP B: Get Outdoor Weather based on a manually forced zip code
void fetch_outdoor_weather(void) {
    char url_buf[128];
    
    // Hardcode your zip code location (95064) directly into the path string,
    // keeping format=1 (raw temperature only) and &m (forces Celsius metric units)
    snprintf(url_buf, sizeof(url_buf), "http://wttr.in/95064?format=1&m");
    
    memset(outdoor_temp_buffer, 0, sizeof(outdoor_temp_buffer));

    esp_http_client_config_t config = {
        .url = url_buf,
        .event_handler = outdoor_http_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    
    // Clean up any trailing newline symbols coming back from wttr.in
    strtok(outdoor_temp_buffer, "\n"); 
}

// 3. STEP C: POST the combined log data back to the server
// void post_combined_data(float onboard_temp) {
//     char url_buf[64];
//     snprintf(url_buf, sizeof(url_buf), "http://%s:1234/", SERVER_IP);

//     char payload[128];
//     snprintf(payload, sizeof(payload), 
//              "Server Location: %s | Outdoor Temp: %s | ESP32 Sensor Temp: %.1fC", 
//              location_buffer, outdoor_temp_buffer, onboard_temp);

//     // Mandated log rule: Both the server and ESP32 must log the combined metrics
//     ESP_LOGI(TAG, "LOCAL LOG DATA: %s", payload);

//     esp_http_client_config_t config = {
//         .url = url_buf,
//         .method = HTTP_METHOD_POST,
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     esp_http_client_set_header(client, "Content-Type", "text/plain");
//     esp_http_client_set_post_field(client, payload, strlen(payload));

//     esp_http_client_perform(client);
//     esp_http_client_cleanup(client);
// }
// 3. STEP C: POST the combined log data back to the server
void post_combined_data(float onboard_temp) {
    char url_buf[64];
    snprintf(url_buf, sizeof(url_buf), "http://%s:1234/", SERVER_IP);

    char payload[128];
    // Formats payload to manually force the clean 95064 text indicator layout
    snprintf(payload, sizeof(payload), 
             "Location: 95064 | Outdoor Temp: %s | ESP32 Sensor Temp: %.1fC", 
             outdoor_temp_buffer, onboard_temp);

    // Both the server and ESP32 will log this information
    ESP_LOGI(TAG, "LOCAL LOG DATA: %s", payload);

    esp_http_client_config_t config = {
        .url = url_buf,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void app_main(void) {
    // Basic System Handshakes
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize On-chip Temp Sensor
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = { .range_min = 0, .range_max = 50 };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // Connect to Wi-Fi Network
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, HOME_SSID);
    strcpy((char *)wifi_config.sta.password, HOME_PASS);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    vTaskDelay(pdMS_TO_TICKS(5000)); // Standard DHCP delay

    while (1) {
        float onboard_temp = 0.0f;
        temperature_sensor_get_celsius(temp_sensor, &onboard_temp);

        // Sequence execution pipeline
        fetch_server_location();
        
        if (strlen(location_buffer) > 0) {
            fetch_outdoor_weather();
            post_combined_data(onboard_temp);
        } else {
            ESP_LOGW(TAG, "Could not resolve server location, retrying link loop...");
        }

        // Run the combined sequence loop every 15 seconds
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}