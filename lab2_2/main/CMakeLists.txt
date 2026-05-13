idf_component_register(SRCS "i2c_basic_example_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_driver_i2c esp_driver_gpio)
