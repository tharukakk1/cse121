/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 * Integrated for CSE121 Lab 4.3 - Full IMU-Driven Motion Mouse
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "driver/i2c_master.h"

#undef CONFIG_EXAMPLE_HID_DEVICE_ROLE
#define CONFIG_EXAMPLE_HID_DEVICE_ROLE 3

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#else
#include "esp_bt_defs.h"
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#endif
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#if CONFIG_BT_SDP_COMMON_ENABLED
#include "esp_sdp_api.h"
#endif 
#endif

#include "esp_hidd.h"
#include "esp_hid_gap.h"

static const char *TAG = "HID_DEV_DEMO";

// --- LAB 4.1 I2C PERIPHERAL TARGET CONFIGURATIONS ---
#define I2C_MASTER_SCL_IO           8           
#define I2C_MASTER_SDA_IO           7           
#define I2C_MASTER_NUM              I2C_NUM_0   
#define I2C_MASTER_FREQ_HZ          400000      
#define I2C_MASTER_TIMEOUT_MS       1000

#define ICM42670_SENSOR_ADDR        0x68        
#define ICM42670_REG_PWR_MGMT0      0x1F        
#define ICM42670_REG_ACCEL_DATA_X1  0x0B        
#define ICM42670_REG_ACCEL_DATA_Y1  0x0D        

// Lab 4.3 Multi-Level Dynamic Speed Thresholds
#define THRESHOLD_LOW               1000        
#define THRESHOLD_HIGH              2000        

#define SPEED_A_BIT                 2
#define SPEED_A_LOT                 8

static i2c_master_dev_handle_t s_imu_dev_handle = NULL;

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
static local_param_t s_ble_hid_param = {0};

const unsigned char mediaReportMap[] = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03, 0x09, 0x02, 0xA1, 0x02, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0A, 0x15, 0x01, 0x25, 0x0A, 0x75, 0x04, 0x95, 0x01, 0x81, 0x00, 0xC0, 0x05, 0x0C, 0x09,
    0x86, 0x15, 0xFF, 0x25, 0x01, 0x75, 0x02, 0x95, 0x01, 0x81, 0x46, 0x09, 0xE9, 0x09, 0xEA, 0x15,
    0x00, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02, 0x09, 0xE2, 0x09, 0x30, 0x09, 0x83, 0x09, 0x81, 0x09,
    0xB0, 0x09, 0xB1, 0x09, 0xB2, 0x09, 0xB3, 0x09, 0xB4, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7, 0x15,
    0x01, 0x25, 0x0C, 0x75, 0x04, 0x95, 0x01, 0x81, 0x00, 0x09, 0x80, 0xA1, 0x02, 0x05, 0x09, 0x19,
    0x01, 0x29, 0x03, 0x15, 0x01, 0x25, 0x03, 0x75, 0x02, 0x81, 0x00, 0xC0, 0x81, 0x03, 0xC0
};

const unsigned char mouseReportMap[] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x03,
    0x81, 0x06, 0xc0, 0xc0
};

void send_mouse(uint8_t buttons, char dx, char dy, char wheel)
{
    static uint8_t buffer[4] = {0};
    buffer[0] = buttons;
    buffer[1] = dx;
    buffer[2] = dy;
    buffer[3] = wheel;
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 0, buffer, 4);
}

// --- IMU HELPER & TASK TRANSPLANTED FROM LAB 4.1 ---
static int16_t local_imu_read_word(uint8_t reg_high_addr)
{
    uint8_t raw_bytes[2] = {0};
    if (s_imu_dev_handle && i2c_master_transmit_receive(s_imu_dev_handle, &reg_high_addr, 1, raw_bytes, 2, I2C_MASTER_TIMEOUT_MS) == ESP_OK) {
        return (int16_t)((raw_bytes[0] << 8) | raw_bytes[1]);
    }
    return 0;
}

void imu_mouse_updater_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Motion Integration Engine Active. Ready to steer.");
    int32_t tilt_ticks_x = 0;
    int32_t tilt_ticks_y = 0;
    const TickType_t poll_interval = pdMS_TO_TICKS(12); 

    while (1) {
        int16_t accel_x = local_imu_read_word(ICM42670_REG_ACCEL_DATA_X1);
        int16_t accel_y = local_imu_read_word(ICM42670_REG_ACCEL_DATA_Y1);

        char out_dx = 0;
        char out_dy = 0;

        if (accel_x < -THRESHOLD_LOW) {
            tilt_ticks_x++;
            char step = (accel_x < -THRESHOLD_HIGH) ? SPEED_A_LOT : SPEED_A_BIT;
            int32_t mult = (tilt_ticks_x > 40) ? 3 : ((tilt_ticks_x > 10) ? 2 : 1);
            out_dx = -(step * mult);
        } else if (accel_x > THRESHOLD_LOW) {
            tilt_ticks_x++;
            char step = (accel_x > THRESHOLD_HIGH) ? SPEED_A_LOT : SPEED_A_BIT;
            int32_t mult = (tilt_ticks_x > 40) ? 3 : ((tilt_ticks_x > 10) ? 2 : 1);
            out_dx = (step * mult);
        } else {
            tilt_ticks_x = 0;
        }

        if (accel_y < -THRESHOLD_LOW) {
            tilt_ticks_y++;
            char step = (accel_y < -THRESHOLD_HIGH) ? SPEED_A_LOT : SPEED_A_BIT;
            int32_t mult = (tilt_ticks_y > 40) ? 3 : ((tilt_ticks_y > 10) ? 2 : 1);
            out_dy = -(step * mult);
        } else if (accel_y > THRESHOLD_LOW) {
            tilt_ticks_y++;
            char step = (accel_y > THRESHOLD_HIGH) ? SPEED_A_LOT : SPEED_A_BIT;
            int32_t mult = (tilt_ticks_y > 40) ? 3 : ((tilt_ticks_y > 10) ? 2 : 1);
            out_dy = (step * mult);
        } else {
            tilt_ticks_y = 0;
        }

        if (out_dx != 0 || out_dy != 0) {
            send_mouse(0, out_dx, out_dy, 0);
        }
        vTaskDelay(poll_interval);
    }
}

void ble_hid_demo_task_mouse(void *pvParameters)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
#endif

#if CONFIG_EXAMPLE_HID_DEVICE_ROLE && CONFIG_EXAMPLE_HID_DEVICE_ROLE == 2
const unsigned char keyboardReportMap[] = { 0x05, 0x01, 0xC0 };
void ble_hid_demo_task_kbd(void *pvParameters) { while(1) vTaskDelay(1000); }
#endif

static esp_hid_raw_report_map_t ble_report_maps[] = {
    { .data = mouseReportMap, .len = sizeof(mouseReportMap) },
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x16C0,
    .product_id         = 0x05DF,
    .version            = 0x0100,
    .device_name        = "HIDM",
    .manufacturer_name  = "Espressif",
    .serial_number      = "1234567890",
    .report_maps        = ble_report_maps,
    .report_maps_len    = 1
};

#if !CONFIG_BT_NIMBLE_ENABLED || CONFIG_EXAMPLE_HID_DEVICE_ROLE == 1
void ble_hid_demo_task(void *pvParameters) { while(1) vTaskDelay(1000); }
#endif

void ble_hid_task_start_up(void)
{
    if (s_ble_hid_param.task_hdl) return;
    xTaskCreate(imu_mouse_updater_task, "imu_mouse_task", 4 * 1024, NULL, configMAX_PRIORITIES - 2, &s_ble_hid_param.task_hdl);
}

void ble_hid_task_shut_down(void)
{
    if (s_ble_hid_param.task_hdl) {
        vTaskDelete(s_ble_hid_param.task_hdl);
        s_ble_hid_param.task_hdl = NULL;
    }
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_DEV_BLE";

    switch (event) {
    case ESP_HIDD_START_EVENT:      ESP_LOGI(TAG, "START"); esp_hid_ble_gap_adv_start(); break;
    case ESP_HIDD_CONNECT_EVENT:    ESP_LOGI(TAG, "CONNECT OK"); break;
    case ESP_HIDD_CONTROL_EVENT: 
        if (param->control.control) ble_hid_task_start_up(); 
        else ble_hid_task_shut_down(); 
        break;
    case ESP_HIDD_DISCONNECT_EVENT: ESP_LOGI(TAG, "DISCONNECT"); ble_hid_task_shut_down(); esp_hid_ble_gap_adv_start(); break;
    default: break;
    }
}

static void local_i2c_bus_init(void)
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ICM42670_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &s_imu_dev_handle));

    uint8_t wake_payload[2] = {ICM42670_REG_PWR_MGMT0, 0x0F};
    ESP_ERROR_CHECK(i2c_master_transmit(s_imu_dev_handle, wake_payload, 2, I2C_MASTER_TIMEOUT_MS));
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "ICM-42670-P Hardware Interface mounted.");
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    local_i2c_bus_init();

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_DEV_MODE);
    ESP_ERROR_CHECK(esp_hid_gap_init(HID_DEV_MODE));

    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_MOUSE, ble_hid_config.device_name);
    ESP_ERROR_CHECK(ret);

#if CONFIG_BT_BLE_ENABLED
    // --- THIS CRUCIAL CALLBACK WAS MISSING ---
    if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "GATTS register callback failed: %d", ret);
        return;
    }
#endif

    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}