#include "DFRobot_LCD.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DFRobot_LCD::DFRobot_LCD(i2c_master_dev_handle_t lcd_handle, i2c_master_dev_handle_t rgb_handle) {
    _lcd_handle = lcd_handle;
    _rgb_handle = rgb_handle;
}

void DFRobot_LCD::writeReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    i2c_master_transmit(dev, write_buf, 2, 1000);
}

void DFRobot_LCD::sendCommand(uint8_t cmd) {
    writeReg(_lcd_handle, 0x80, cmd); 
}

void DFRobot_LCD::sendData(uint8_t data) {
    writeReg(_lcd_handle, 0x40, data); 
}

void DFRobot_LCD::init() {
    vTaskDelay(pdMS_TO_TICKS(50)); 

    // Initialize character matrix layout
    sendCommand(0x38); // 8-bit mode, 2 lines
    vTaskDelay(pdMS_TO_TICKS(5));
    sendCommand(0x0C); // Display ON
    sendCommand(0x01); // Clear screen
    vTaskDelay(pdMS_TO_TICKS(5));
    sendCommand(0x06); // Auto-increment cursor
    
    // Wake up background RGB controller chip
    writeReg(_rgb_handle, 0x00, 0x00); 
    writeReg(_rgb_handle, 0x08, 0xFF); 
}

void DFRobot_LCD::setCursor(uint8_t col, uint8_t row) {
    uint8_t val = (row == 0) ? (0x80 + col) : (0xC0 + col);
    sendCommand(val);
}

void DFRobot_LCD::printstr(const char* str) {
    while (*str) {
        sendData(*str++);
    }
}

void DFRobot_LCD::setRGB(uint8_t r, uint8_t g, uint8_t b) {
    writeReg(_rgb_handle, 0x04, r); 
    writeReg(_rgb_handle, 0x03, g); 
    writeReg(_rgb_handle, 0x02, b); 
}