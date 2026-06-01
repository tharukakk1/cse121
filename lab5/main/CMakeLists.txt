# main/CMakeLists.txt
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_rom esp_driver_gpio esp_driver_tsens)