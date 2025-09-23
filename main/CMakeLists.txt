idf_component_register(SRCS 
                       "main.c"
                       "i2c_components/src/i2c_components.c"
                       PRIV_REQUIRES spi_flash unity bme280
                       INCLUDE_DIRS
                       "."
                       "i2c_components/include")
