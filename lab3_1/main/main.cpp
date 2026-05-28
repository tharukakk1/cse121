#include "DFRobot_LCD.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RGB_ADDRESS 0x2D 

extern "C" void app_main(void) {
    DFRobot_LCD lcd(RGB_ADDRESS);
    lcd.init();
    
    // Choose a nice background color pairing (e.g., solid teal/white)
    lcd.setRGB(0, 128, 255); 
    
    // Print required string on Row 1
    lcd.setCursor(0, 0);
    lcd.printstr("Hello CSE121!");
    
    // Print required string on Row 2 (Replace with your actual last name!)
    lcd.setCursor(0, 1);
    lcd.printstr("Kodituwakku"); 
    
    // Keep running without doing anything further to preserve the display state
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}