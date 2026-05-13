/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           8 //CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           7 //CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000                      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define SHTC 0x70
#define PWRDN 0xB098
#define PWRUP 0x3517
#define READSHTC 0x7866

/**
 * @brief Read a sequence of bytes from a MPU9250 sensor registers
 */
static esp_err_t mpu9250_register_read(i2c_master_dev_handle_t dev_handle, /*uint8_t reg_addr,*/ uint8_t *data, size_t len)
{
    return i2c_master_receive(dev_handle, /*&reg_addr, 1,*/ data, len, I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Write a byte to a MPU9250 sensor register
 */
static esp_err_t write(i2c_master_dev_handle_t dev_handle, /*uint8_t reg_addr,*/ uint16_t data)
{
    uint8_t high_byte = (uint8_t)(data >> 8);
    uint8_t low_byte  = (uint8_t)(data & 0xFF);
    uint8_t write_buf[2] = {high_byte, low_byte};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Convert raw sensor data to Celsius temperature
 * @param data Pointer to 2-3 bytes of raw temperature data
 * @return Temperature in Celsius
 */
static float convert_to_celsius(uint8_t *data)
{
    uint16_t raw = (data[0] << 8) | data[1];
    return raw / 65536.0 * 175 - 45;
}

/**
 * @brief Convert Celsius temperature to Fahrenheit
 * @param celsius Temperature in Celsius
 * @return Temperature in Fahrenheit
 */
static float convert_to_fahrenheit(float celsius)
{
    return celsius * 9.0 / 5.0 + 32.0;
}

/**
 * @brief Convert raw sensor data to humidity percentage
 * @param data Pointer to 2-3 bytes of raw humidity data
 * @return Humidity as percentage (0-100)
 */
static float convert_to_humidity(uint8_t *data)
{
    uint16_t raw = (data[0] << 8) | data[1];
    return raw / 65536.0 * 100;
}

/**
 * @brief Calculate CRC-8 checksum for SHTC3 sensor
 * @param data Pointer to 2 bytes of data
 * @return CRC-8 checksum value
 */
static uint8_t calculate_crc8(uint8_t *data)
{
    uint8_t crc = 0xFF; // Initial value
    const uint8_t POLYNOMIAL = 0x31;

    for (int i = 0; i < 2; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ POLYNOMIAL;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Verify checksum of SHTC3 sensor data
 * @param data Pointer to 3 bytes (2 data bytes + 1 CRC byte)
 * @return true if checksum is valid, false otherwise
 */
static bool verify_checksum(uint8_t *data)
{
    uint8_t expected_crc = calculate_crc8(data);
    uint8_t actual_crc = data[2];
    return expected_crc == actual_crc;
}

/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 0,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
    // Toggle SCL a few times to clear any stuck state
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_OUTPUT);
    for(int i = 0; i < 9; i++) {
        gpio_set_level(I2C_MASTER_SCL_IO, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(I2C_MASTER_SCL_IO, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Let it stabilize

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    vTaskDelay(pdMS_TO_TICKS(500));

    while(true){
        // 1. Power Up
        write(dev_handle, PWRUP);
        vTaskDelay(pdMS_TO_TICKS(5));
        
        // 2. Measure
        write(dev_handle, READSHTC);

        // 3. WAIT - The SHTC3 is doing the conversion. 
        // If you read too early, you get F1.
        vTaskDelay(pdMS_TO_TICKS(50)); // Give it plenty of time (50ms)

        // 4. Read
        uint8_t raw[6] = {0};
        esp_err_t err = i2c_master_receive(dev_handle, raw, 6, I2C_MASTER_TIMEOUT_MS);

        if (err == ESP_OK) {
            printf("Raw: %02X %02X %02X %02X %02X %02X\n", raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
            // Verify checksums for temperature and humidity data
            bool temp_crc_valid = verify_checksum(&raw[0]);
            bool hum_crc_valid = verify_checksum(&raw[3]);

            if (temp_crc_valid && hum_crc_valid) {
                float temp_c = convert_to_celsius(&raw[0]);
                float temp_f = convert_to_fahrenheit(temp_c);
                float hum = convert_to_humidity(&raw[3]);
                ESP_LOGI(TAG, "Temperature: %.2f °C, %.2f °F, Humidity: %.2f %%", temp_c, temp_f, hum);
            } else {
                if (!temp_crc_valid) {
                    ESP_LOGW(TAG, "Temperature checksum validation failed [%02X %02X %02X]", raw[0], raw[1], raw[2]);
                }
                if (!hum_crc_valid) {
                    ESP_LOGW(TAG, "Humidity checksum validation failed [%02X %02X %02X]", raw[3], raw[4], raw[5]);
                }
            }
        } else {
            ESP_LOGE(TAG, "Failed to read from sensor: %s", esp_err_to_name(err));
        }

        // 5. Power Down
        // write(dev_handle, PWRDN);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    ESP_LOGI(TAG, "I2C de-initialized successfully");
}
