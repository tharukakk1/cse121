#ifndef DFROBOT_LCD_H
#define DFROBOT_LCD_H

#include <stdint.h>
#include "driver/i2c.h"

#define LCD_ADDRESS     0x3E 

class DFRobot_LCD {
public:
    DFRobot_LCD(uint8_t rgbAddr); 
    void init();
    void setCursor(uint8_t col, uint8_t row);
    void printstr(const char* str);
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
    uint8_t _rgbAddr;
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void writeReg(uint8_t i2cAddr, uint8_t reg, uint8_t data);
};

#endif