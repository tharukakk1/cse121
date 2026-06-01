/*
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 * Adapted for CSE121 Lab 4.1 - Board Movement Tracking
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "LAB4_1";

// ESP32-C3 DevKit-Rust-2 Hardware Pin Specifications
#define I2C_MASTER_SCL_IO           8           /*!< GPIO08 used for Rust-2 I2C clock */
#define I2C_MASTER_SDA_IO           7           /*!< GPIO07 used for Rust-2 I2C data  */
#define I2C_MASTER_NUM              I2C_NUM_0   
#define I2C_MASTER_FREQ_HZ          400000      /*!< I2C master clock frequency (400kHz) */
#define I2C_MASTER_TIMEOUT_MS       1000

// ICM-42670-P Hardware Constraints
#define ICM42670_SENSOR_ADDR        0x68        /*!< Slave I2C address of the ICM-42670-P IMU */
#define ICM42670_REG_PWR_MGMT0      0x1F        /*!< Power Management register to enable sensor */
#define ICM42670_REG_ACCEL_DATA_X1  0x0B        /*!< Accelerometer X-axis High Byte address */
#define ICM42670_REG_ACCEL_DATA_Y1  0x0D        /*!< Accelerometer Y-axis High Byte address */

// Calibrated Tilt Threshold (Adjust this value if detection is too twitchy or stiff)
#define TILT_THRESHOLD              1000

/**
 * @brief Helper to read consecutive bytes from the sensor registers
 */
static esp_err_t imu_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Helper to write a single byte to an IMU register
 */
static esp_err_t imu_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Helper to read a combined 16-bit signed integer value from consecutive high/low registers
 */
static int16_t imu_read_word(i2c_master_dev_handle_t dev_handle, uint8_t reg_high_addr)
{
    uint8_t raw_bytes[2] = {0};
    // Read 2 consecutive bytes starting from the high byte address
    if (imu_register_read(dev_handle, reg_high_addr, raw_bytes, 2) == ESP_OK) {
        // Combine raw_bytes[0] (High Byte) and raw_bytes[1] (Low Byte) into a signed short int
        return (int16_t)((raw_bytes[0] << 8) | raw_bytes[1]);
    }
    return 0;
}

/**
 * @brief Modern I2C Master initialization using the bus-device framework
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ICM42670_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    // 1. Initialize the modern I2C master bus structure
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully on Rust-2 pins (SDA:7, SCL:8)");

    // 2. Wake up and configure the ICM-42670-P Accelerometer
    // Writing 0x0F enables both Accelerometer and Gyroscope in Low Noise Mode (Continuous sampling)
    ESP_ERROR_CHECK(imu_register_write_byte(dev_handle, ICM42670_REG_PWR_MGMT0, 0x0F));
    vTaskDelay(pdMS_TO_TICKS(50)); // Short pause to stabilize power rails
    
    ESP_LOGI(TAG, "IMU active. Testing board tracking...");

    // 3. Continuous Polling Loop
    while (true) {
        // Fetch raw gravitational data values
        int16_t accel_x = imu_read_word(dev_handle, ICM42670_REG_ACCEL_DATA_X1);
        int16_t accel_y = imu_read_word(dev_handle, ICM42670_REG_ACCEL_DATA_Y1);

        char output_str[32] = "";
        bool has_y = false;
        bool has_x = false;

        // Evaluate Y-axis tilt (UP / DOWN mapping based on standard mounting)
        if (accel_y < -TILT_THRESHOLD) {
            snprintf(output_str, sizeof(output_str), "UP");
            has_y = true;
        } else if (accel_y > TILT_THRESHOLD) {
            snprintf(output_str, sizeof(output_str), "DOWN");
            has_y = true;
        }

        // Evaluate X-axis tilt (LEFT / RIGHT mapping)
        if (accel_x < -TILT_THRESHOLD) {
            if (has_y) strcat(output_str, " "); // Insert separating space for combined labels
            strcat(output_str, "LEFT");
            has_x = true;
        } else if (accel_x > TILT_THRESHOLD) {
            if (has_y) strcat(output_str, " ");
            strcat(output_str, "RIGHT");
            has_x = true;
        }

        // Lab 4.1 Requirement: Log only when a threshold condition is met using ESP_LOGI
        if (has_y || has_x) {
            ESP_LOGI(TAG, "%s", output_str);
        }

        vTaskDelay(pdMS_TO_TICKS(150)); // Check tilt status every 150ms
    }
}