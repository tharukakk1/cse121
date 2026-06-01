#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"                    // For precise microsecond timing (ets_delay_us) 
#include "driver/temperature_sensor.h"  // For the ESP32-C3's internal temperature sensor 

static const char *TAG = "ULTRASONIC_LAB";

// Pin assignments for your Rust 2.0 board
#define TRIG_PIN GPIO_NUM_4
#define ECHO_PIN GPIO_NUM_5

// Global handle for the internal temperature sensor peripheral
temperature_sensor_handle_t temp_sensor = NULL;

void init_hardware(void) {
    // 1. Configure Trigger Pin as Output
    gpio_reset_pin(TRIG_PIN);
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG_PIN, 0);

    // 2. Configure Echo Pin as Input
    gpio_reset_pin(ECHO_PIN);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);

    // 3. Configure ESP32-C3 Internal Temperature Sensor 
    // The lab guide outlines a standard range expectation of 0°C to 50°C [cite: 26]
    temperature_sensor_config_t temp_sensor_config = {
        .range_min = 0,
        .range_max = 50,
    };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
}

void app_main(void) {
    init_hardware();
    ESP_LOGI(TAG, "Hardware initialized successfully.");

    while (1) {
        // 1. Read the live internal chip temperature 
        float tsens_out = 23.0f; // Default fallback temperature matching sample text 
        if (temperature_sensor_get_celsius(temp_sensor, &tsens_out) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read internal temperature sensor, defaulting calculation to 23C.");
        }

        // 2. Compute the dynamic speed of sound based on air temperature (m/s)
        // Physics formula: c = 331.3 + (0.606 * Temperature)
        float speed_of_sound = 331.3f + (0.606f * tsens_out);

        // 3. Trigger the US-100 by raising TRIG high for exactly 10 microseconds [cite: 49]
        gpio_set_level(TRIG_PIN, 1);
        ets_delay_us(10); // Precise hardware timer delay 
        gpio_set_level(TRIG_PIN, 0);

        // 4. Measure the duration of the incoming Echo response pulse
        // Wait up to a fixed threshold for the ECHO pin to go HIGH
        uint32_t timeout_counter = 0;
        while (gpio_get_level(ECHO_PIN) == 0) {
            ets_delay_us(1);
            timeout_counter++;
            if (timeout_counter > 30000) break; // Timeout after ~30ms if no object found
        }

        uint32_t echo_duration_us = 0;
        if (timeout_counter <= 30000) {
            // Echo line has gone high! Time the pulse until it falls back to LOW
            while (gpio_get_level(ECHO_PIN) == 1) {
                ets_delay_us(1);
                echo_duration_us++;
                // Security safety ceiling to break if the line gets hung high
                if (echo_duration_us > 40000) break; 
            }
        }

        // 5. Calculate distance using calculated speed of sound and round-trip microsecond window
        // Distance = (Time in seconds * Speed of Sound in m/s) / 2
        // To get cm: Distance = ((Time_us / 1000000) * (Speed_of_sound * 100)) / 2
        float distance_cm = 0.0f;
        if (echo_duration_us > 0) {
            distance_cm = ((float)echo_duration_us / 1000000.0f) * (speed_of_sound * 100.0f) / 2.0f;
        }

        // 6. Format and log findings directly to terminal output exactly as requested [cite: 27, 28]
        // Example syntax required: Distance 3.5cm at 23C 
        printf("Distance: %.1fcm at %dC\n", distance_cm, (int)tsens_out);

        // Enforce a structured 1-second update cadence outside the critical loop sequence [cite: 27]
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}