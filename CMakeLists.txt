#
# CMakeLists.txt file for Core2 for AWS IoT Kit Board Support Package
#
cmake_minimum_required( VERSION 3.16.0 )

set( ESP_IDF_PATH                       ${idf_path} )
set( CORE2FORAWS_SRCS                   "${COMPONENT_DIR}/core2foraws.c" )
set( CORE2FORAWS_INCLUDE_DIRS           "${COMPONENT_DIR}/include" )

# esp_adc_cal was removed in ESP-IDF v5.x, replaced by esp_adc
if( IDF_VERSION_MAJOR GREATER_EQUAL 5 )
    set( ADC_COMPONENT "esp_adc" )
else()
    set( ADC_COMPONENT "esp_adc_cal" )
endif()

set( CORE2FORAWS_REQUIRES               ${ADC_COMPONENT}
                                        "mbedtls"
                                        "freertos"
                                        "driver"
                                        "esp_driver_i2c"
                                        "esp_driver_i2s"
                                        "esp_driver_rmt"
                                        "esp_timer"
                                        "esp-cryptoauthlib"
                                        "fatfs"
                                        )
set( CORE2FORAWS_PRIV_INCLUDEDIRS       )
set( CORE2FORAWS_PRIV_REQUIRES          "nvs_flash" 
                                        "wifi_provisioning"
                                        )
set( CORE2FORAWS_EXCLUDE_SRCS           )
set( CORE2FORAWS_LIB                    "${COMPONENT_DIR}/lib" )


if( CONFIG_SOFTWARE_BSP_SUPPORT )
    set( CORE2FORAWS_BSP_SRCS                   "${CORE2FORAWS_LIB}/common/core2foraws_common.c"
                                                "${CORE2FORAWS_LIB}/common/core2foraws_i2c.c"
                                                "${CORE2FORAWS_LIB}/power/core2foraws_power.c"
                                                "${CORE2FORAWS_LIB}/power/axp192.c"
                                                )
    set( CORE2FORAWS_BSP_INCLUDE_DIRS           "${CORE2FORAWS_LIB}/common/include" 
                                                "${CORE2FORAWS_LIB}/power/include"
                                                )
    list( APPEND CORE2FORAWS_SRCS               ${CORE2FORAWS_BSP_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS       ${CORE2FORAWS_BSP_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_AUDIO_SUPPORT )
    set( CORE2FORAWS_AUDIO_SRCS             "${CORE2FORAWS_LIB}/audio/core2foraws_audio.c" )
    set( CORE2FORAWS_AUDIO_INCLUDE_DIRS     "${CORE2FORAWS_LIB}/audio/include/" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_AUDIO_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_AUDIO_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_BUTTON_SUPPORT )
    set( CORE2FORAWS_BUTTON_SRCS            "${CORE2FORAWS_LIB}/button/core2foraws_button.c" )
    set( CORE2FORAWS_BUTTON_INCLUDE_DIRS    "${CORE2FORAWS_LIB}/button/include/" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_BUTTON_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_BUTTON_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_CRYPTO_SUPPORT )
    set( CORE2FORAWS_CRYPTO_SRCS            "${CORE2FORAWS_LIB}/crypto/core2foraws_crypto.c"
                                            "${CORE2FORAWS_LIB}/crypto/core2foraws_crypto_hal.c"
                                            )
    set( CORE2FORAWS_CRYPTO_INCLUDE_DIRS    "${CORE2FORAWS_LIB}/crypto/include" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_CRYPTO_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_CRYPTO_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_DISPLAY_SUPPORT )
    set( CORE2FORAWS_DISPLAY_SRCS           "${CORE2FORAWS_LIB}/display/core2foraws_display.c" )
    set( CORE2FORAWS_DISPLAY_INCLUDE_DIRS   "${CORE2FORAWS_LIB}/display/include" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_DISPLAY_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_DISPLAY_INCLUDE_DIRS} )
    list( APPEND CORE2FORAWS_REQUIRES       "esp_lcd"
                                              "esp_driver_spi" )
endif()


if( CONFIG_SOFTWARE_EXPANSION_PORTS_SUPPORT )
    set( CORE2FORAWS_EXPPORTS_SRCS          "${CORE2FORAWS_LIB}/expports/core2foraws_expports.c" )
    set( CORE2FORAWS_EXPPORTS_INCLUDE_DIRS  "${CORE2FORAWS_LIB}/expports/include/")

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_EXPPORTS_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_EXPPORTS_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_MOTION_SUPPORT )
    set( CORE2FORAWS_MOTION_SRCS                "${CORE2FORAWS_LIB}/motion/core2foraws_motion.c" 
                                                "${CORE2FORAWS_LIB}/motion/mpu6886.c"
                                                )
    set( CORE2FORAWS_MOTION_INCLUDE_DIRS        "${CORE2FORAWS_LIB}/motion/include"
                                                )
    list( APPEND CORE2FORAWS_SRCS               ${CORE2FORAWS_MOTION_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS       ${CORE2FORAWS_MOTION_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_RGB_LED_SUPPORT )
    file( GLOB CORE2FORAWS_RGB_LED_SRCS     "${CORE2FORAWS_LIB}/rgb_led/*.c" )
    set( CORE2FORAWS_RGB_LED_INCLUDE_DIRS   "${CORE2FORAWS_LIB}/rgb_led/include" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_RGB_LED_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_RGB_LED_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_RTC_SUPPORT )
    set( CORE2FORAWS_RTC_SRCS               "${CORE2FORAWS_LIB}/rtc/core2foraws_rtc.c" )
    set( CORE2FORAWS_RTC_INCLUDE_DIRS       "${CORE2FORAWS_LIB}/rtc/include" )
    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_RTC_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_RTC_INCLUDE_DIRS} )
endif()


if( CONFIG_SOFTWARE_SDCARD_SUPPORT )
    set( CORE2FORAWS_SD_SRCS               "${CORE2FORAWS_LIB}/sd/core2foraws_sd.c" )
    set( CORE2FORAWS_SD_INCLUDE_DIRS       "${CORE2FORAWS_LIB}/sd/include" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_SD_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_SD_INCLUDE_DIRS} )
endif()

if( CONFIG_SOFTWARE_WIFI_SUPPORT )
    set( CORE2FORAWS_WIFI_SRCS              "${CORE2FORAWS_LIB}/wifi/core2foraws_wifi.c" )
    set( CORE2FORAWS_WIFI_INCLUDE_DIRS      "${CORE2FORAWS_LIB}/wifi/include" )

    list( APPEND CORE2FORAWS_SRCS           ${CORE2FORAWS_WIFI_SRCS} )
    list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${CORE2FORAWS_WIFI_INCLUDE_DIRS} )

    if( IDF_VERSION_MAJOR GREATER_EQUAL 5 )
        # ESP-IDF v5+: qrcode is a managed component (espressif/qrcode)
        list( APPEND CORE2FORAWS_PRIV_REQUIRES  "qrcode" )
    else()
        # ESP-IDF v4: qrcode ships in examples/common_components
        file( GLOB_RECURSE QRCODE_SRCS          "${ESP_IDF_PATH}/examples/common_components/qrcode/*.c" )
        set( QRCODE_INCLUDE_DIRS                "${ESP_IDF_PATH}/examples/common_components/qrcode"
                                                "${ESP_IDF_PATH}/examples/common_components/qrcode/include"
                                                )
        list( APPEND CORE2FORAWS_SRCS           ${QRCODE_SRCS} )
        list( APPEND CORE2FORAWS_INCLUDE_DIRS   ${QRCODE_INCLUDE_DIRS} )
    endif()
endif()

idf_component_register( SRCS                ${CORE2FORAWS_SRCS}
                        INCLUDE_DIRS        ${CORE2FORAWS_INCLUDE_DIRS}
                        PRIV_INCLUDE_DIRS   ${CORE2FORAWS_PRIV_INCLUDEDIRS}
                        PRIV_REQUIRES       ${CORE2FORAWS_PRIV_REQUIRES}
                        EXCLUDE_SRCS        ${CORE2FORAWS_EXCLUDE_SRCS}
                        REQUIRES            ${CORE2FORAWS_REQUIRES}
                        )

# Override esp-cryptoauthlib's default I2C HAL so the ATECC608 uses the BSP's
# shared internal I2C bus instead of creating its own.
if( CONFIG_SOFTWARE_CRYPTO_SUPPORT )
    target_link_options( ${COMPONENT_LIB} PUBLIC
        -Wl,--wrap=hal_i2c_init
        -Wl,--wrap=hal_i2c_post_init
        -Wl,--wrap=hal_i2c_send
        -Wl,--wrap=hal_i2c_receive
        -Wl,--wrap=hal_i2c_release
        -Wl,--wrap=hal_i2c_change_baud
        -Wl,--wrap=hal_i2c_wake
        -Wl,--wrap=hal_i2c_control
    )
endif()






