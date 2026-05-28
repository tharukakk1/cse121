#include <stdio.h>
#include "DFRobot_LCD.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO    8
#define I2C_MASTER_SDA_IO    7 
#define I2C_MASTER_NUM       I2C_NUM_0

#define SHTC_ADDR            0x70
#define LCD_ADDR             0x3E
#define RGB_ADDR             0x2D 

#define CMD_PWRUP            0x3517
#define CMD_PWRDN            0xB098
#define CMD_MEASURE          0x7866

static const char *TAG = "LAB_3_2";

// Conversion functions from your verified implementation
static float convert_to_celsius(uint8_t *data) {
    uint16_t raw = (data[0] << 8) | data[1];
    return (raw / 65536.0f) * 175.0f - 45.0f;
}

static uint8_t calculate_crc8(uint8_t *data) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < 2; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc = crc << 1;
        }
    }
    return crc;
}

static bool verify_checksum(uint8_t *data) {
    return calculate_crc8(data) == data[2];
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing shared modern I2C master bus...");

    // 1. Configure the central Shared I2C Bus Master Config safely for C++
    i2c_master_bus_config_t bus_config = {}; 
    bus_config.i2c_port = I2C_MASTER_NUM;
    bus_config.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    bus_config.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 0;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // 2. Clear out the device config base struct layout cleanly
    i2c_device_config_t dev_conf = {}; 
    dev_conf.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_conf.scl_speed_hz = 100000;
    
    i2c_master_dev_handle_t shtc_handle, lcd_handle, rgb_handle;
    
    dev_conf.device_address = SHTC_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_conf, &shtc_handle));
    
    dev_conf.device_address = LCD_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_conf, &lcd_handle));
    
    dev_conf.device_address = RGB_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_conf, &rgb_handle));

    // 3. Start the display engine
    DFRobot_LCD lcd(lcd_handle, rgb_handle);
    lcd.init();
    lcd.setRGB(0, 150, 255); // Solid background theme

    char line_buf[16];
    ESP_LOGI(TAG, "Bus initialization complete. Entering execution loop...");

    while (true) {
        // 1. Power Up Sequence
        uint8_t pwr_up_buf[2] = { (uint8_t)(CMD_PWRUP >> 8), (uint8_t)(CMD_PWRUP & 0xFF) };
        i2c_master_transmit(shtc_handle, pwr_up_buf, 2, 1000);
        
        // INCREASE DELAY: Give the physical SHTC3 internal logic extra time to stabilize after wake
        vTaskDelay(pdMS_TO_TICKS(20)); 

        // 2. Send Measurement Instruction
        uint8_t meas_buf[2] = { (uint8_t)(CMD_MEASURE >> 8), (uint8_t)(CMD_MEASURE & 0xFF) };
        i2c_master_transmit(shtc_handle, meas_buf, 2, 1000);
        
        // Conversion wait window
        vTaskDelay(pdMS_TO_TICKS(50)); 

        // 3. Read Data Payload Bytes
        uint8_t raw[6] = {0};
        esp_err_t err = i2c_master_receive(shtc_handle, raw, 6, 1000);

        if (err == ESP_OK) {
            if (verify_checksum(&raw[0]) && verify_checksum(&raw[3])) {
                int temp_c = (int)(convert_to_celsius(&raw[0]) + 0.5f); 
                int humidity = (int)(((raw[3] << 8) | raw[4]) / 65536.0f * 100.0f + 0.5f);

                // Row 1: Temperature
                snprintf(line_buf, sizeof(line_buf), "Temp: %dC      ", temp_c);
                lcd.setCursor(0, 0);
                lcd.printstr(line_buf);

                // Row 2: Humidity
                snprintf(line_buf, sizeof(line_buf), "Hum : %d%%      ", humidity);
                lcd.setCursor(0, 1);
                lcd.printstr(line_buf);
            } else {
                // Checksum debug message
                ESP_LOGW(TAG, "CRC Checksum Failed! Raw: %02X %02X %02X %02X %02X %02X", 
                         raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
            }
        } else {
            ESP_LOGE(TAG, "I2C Receive Error: %s", esp_err_to_name(err));
        }

        // 4. Power Down to satisfy low power lifecycle rule
        uint8_t pwr_dn_buf[2] = { (uint8_t)(CMD_PWRDN >> 8), (uint8_t)(CMD_PWRDN & 0xFF) };
        i2c_master_transmit(shtc_handle, pwr_dn_buf, 2, 1000);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Update once each second [cite: 445]
    }
}