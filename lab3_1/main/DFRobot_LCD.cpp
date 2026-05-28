#include "DFRobot_LCD.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ         100000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

#define I2C_MASTER_SCL_IO          GPIO_NUM_8  
#define I2C_MASTER_SDA_IO          GPIO_NUM_7  

DFRobot_LCD::DFRobot_LCD(uint8_t rgbAddr) {
    _rgbAddr = rgbAddr;
}

void DFRobot_LCD::writeReg(uint8_t i2cAddr, uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    i2c_master_write_to_device(I2C_MASTER_NUM, i2cAddr, write_buf, 2, pdMS_TO_TICKS(1000));
}

void DFRobot_LCD::sendCommand(uint8_t cmd) {
    writeReg(LCD_ADDRESS, 0x80, cmd); 
}

void DFRobot_LCD::sendData(uint8_t data) {
    writeReg(LCD_ADDRESS, 0x40, data); 
}

void DFRobot_LCD::init() {
    // Initialize the ESP32-C3 I2C peripheral hardware
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);

    vTaskDelay(pdMS_TO_TICKS(50)); 

    // Command sequence to initialize the LCD character matrix
    sendCommand(0x38); // Function set: 8-bit mode, 2 display lines
    vTaskDelay(pdMS_TO_TICKS(5));
    sendCommand(0x0C); // Display ON, Cursor OFF, Blink OFF
    sendCommand(0x01); // Clear display memory command
    vTaskDelay(pdMS_TO_TICKS(5));
    sendCommand(0x06); // Entry mode set: increment cursor automatically
    
    // Wake up your V2.0 background RGB controller chip
    writeReg(_rgbAddr, 0x00, 0x00); 
    writeReg(_rgbAddr, 0x08, 0xFF); 
}

void DFRobot_LCD::setCursor(uint8_t col, uint8_t row) {
    // Map column and row offsets straight to DDRAM addresses
    uint8_t val = (row == 0) ? (0x80 + col) : (0xC0 + col);
    sendCommand(val);
}

void DFRobot_LCD::printstr(const char* str) {
    while (*str) {
        sendData(*str++);
    }
}

void DFRobot_LCD::setRGB(uint8_t r, uint8_t g, uint8_t b) {
    writeReg(_rgbAddr, 0x04, r); 
    writeReg(_rgbAddr, 0x03, g); 
    writeReg(_rgbAddr, 0x02, b); 
}