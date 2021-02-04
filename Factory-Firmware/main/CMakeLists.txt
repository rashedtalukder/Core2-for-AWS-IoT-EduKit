set(SOURCES main.c)
idf_component_register(SRCS main.c home.c wifi.c led_bar.c mpu.c mic.c sound.c clock.c power.c touch.c  crypto.c
                    INCLUDE_DIRS . images sounds includes
                    REQUIRES core2forAWS esp-cryptoauthlib fft nvs_flash)

add_compile_definitions(FIRMWARE_VERSION="1.1.0")