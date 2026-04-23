#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           19      // SCL Pin
#define I2C_MASTER_SDA_IO           18      // SDA Pin
#define I2C_MASTER_NUM              0      // I2C port number
#define I2C_MASTER_FREQ_HZ          100000 // 100kHz
#define SHTC3_ADDR                  0x70   // Sensor address

// SHTC3 Commands
#define SHTC3_CMD_WAKEUP            0x3517
#define SHTC3_CMD_SLEEP             0xB098
#define SHTC3_CMD_MEASURE_T_FIRST   0x7866
#define SHTC3_CMD_MEASURE_H_FIRST   0x58E0

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

esp_err_t shtc3_send_command(uint16_t command) {
    uint8_t cmd_data[2] = {(command >> 8) & 0xFF, command & 0xFF};
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_ADDR, cmd_data, 2, pdMS_TO_TICKS(1000));
}

void shtc3_power_up() {
    shtc3_send_command(SHTC3_CMD_WAKEUP);
    vTaskDelay(pdMS_TO_TICKS(1)); // Wait for wakeup (~240us min)
}

void shtc3_power_down() {
    shtc3_send_command(SHTC3_CMD_SLEEP);
}

uint8_t crc8(uint8_t data[], int len) {
    uint8_t crc = 0xFF; 
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

float read_temperature() {
    uint8_t data[3];
    shtc3_send_command(SHTC3_CMD_MEASURE_T_FIRST);
    vTaskDelay(pdMS_TO_TICKS(20)); // Wait for measurement

    i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_ADDR, data, 3, pdMS_TO_TICKS(1000));

    if (crc8(data, 2) != data[2]) {
        ESP_LOGE("SHTC3", "CRC Checksum failed for Temperature!");
        return -999.0;
    }

    uint16_t raw_t = (data[0] << 8) | data[1];
    return -45.0 + 175.0 * ((float)raw_t / 65536.0);
}

float read_humidity() {
    uint8_t data[3];
    shtc3_send_command(SHTC3_CMD_MEASURE_H_FIRST);
    vTaskDelay(pdMS_TO_TICKS(20)); // Wait for measurement

    i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_ADDR, data, 3, pdMS_TO_TICKS(1000));

    if (crc8(data, 2) != data[2]) {
        ESP_LOGE("SHTC3", "CRC Checksum failed for Humidity!");
        return -999.0;
    }

    uint16_t raw_h = (data[0] << 8) | data[1];
    return 100.0 * ((float)raw_h / 65536.0);
}

void app_main(void) {
    ESP_ERROR_CHECK(i2c_master_init());

    while (1) {
        // 1. Power Up (Requirement: Call between every read)
        shtc3_power_up();

        // 2. Read Temperature (Requirement: Separate function, max 3 bytes)
        float temp_c = read_temperature();

        // 3. Read Humidity (Requirement: Separate function, max 3 bytes)
        float humidity = read_humidity();

        // 4. Power Down (Requirement: Call between every read)
        shtc3_power_down();

        // 5. Final Print Statement (Requirement: Specific formatting)
        if (temp_c != -999.0 && humidity != -999.0) {
            float temp_f = (temp_c * 1.8) + 32;
            printf("Temperature is %.0fC (or %.0fF) with a %.0f%% humidity\n", 
                    temp_c, temp_f, humidity);
        } else {
            printf("Sensor Error: Check connections or CRC.\n");
        }

        // 6. Wait exactly 2 seconds (Requirement: Read once every 2 seconds)
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}