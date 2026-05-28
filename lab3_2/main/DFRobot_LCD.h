#ifndef DFROBOT_LCD_H
#define DFROBOT_LCD_H

#include <stdint.h>
#include "driver/i2c_master.h" // Swap to modern driver header

#define LCD_ADDRESS     0x3E 

class DFRobot_LCD {
public:
    // Pass the active device handle straight in
    DFRobot_LCD(i2c_master_dev_handle_t lcd_handle, i2c_master_dev_handle_t rgb_handle); 
    void init();
    void setCursor(uint8_t col, uint8_t row);
    void printstr(const char* str);
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
    i2c_master_dev_handle_t _lcd_handle;
    i2c_master_dev_handle_t _rgb_handle;
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void writeReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t data);
};

#endif