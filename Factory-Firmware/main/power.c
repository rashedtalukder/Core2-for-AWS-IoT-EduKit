/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.0.0
 * power.c
 * 
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2forAWS.h"
#include "lvgl/lvgl.h"
#include "axp192.h"

#include "power.h"

static void led_event_handler(lv_obj_t *obj, lv_event_t event);
static void vibration_event_handler(lv_obj_t *obj, lv_event_t event);
static void brightness_event_handler(lv_obj_t *slider, lv_event_t event);

static const char *TAG = POWER_TAB_NAME;

lv_obj_t *power_tab;
TaskHandle_t power_handle;

void display_power_tab(lv_obj_t *tv, lv_obj_t *core2forAWS_screen_obj){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    power_tab = lv_tabview_add_tab(tv, POWER_TAB_NAME);  // Create a tab

    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_obj_t *power_bg = lv_obj_create(power_tab, NULL);
    lv_obj_align(power_bg, NULL, LV_ALIGN_IN_TOP_LEFT, 16, 36);
    lv_obj_set_size(power_bg, 290, 190);
    lv_obj_set_click(power_bg, false);
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(157, 224, 52));
    lv_obj_add_style(power_bg, LV_OBJ_PART_MAIN, &bg_style);

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_obj_t *tab_title_lbl = lv_label_create(power_bg, NULL);
    lv_obj_add_style(tab_title_lbl, LV_OBJ_PART_MAIN, &title_style);
    lv_label_set_static_text(tab_title_lbl, "AXP192 Power Mgmt");
    lv_obj_align(tab_title_lbl, power_bg, LV_ALIGN_IN_TOP_MID, 0, 10);

    /* Create the sensor information label object */
    lv_obj_t *body_lbl = lv_label_create(power_bg, NULL);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_BREAK);
    lv_label_set_static_text(body_lbl, "The AXP192 provides power management for the battery and on-board peripherals.");
    lv_obj_set_width(body_lbl, 252);
    lv_obj_align(body_lbl, power_bg, LV_ALIGN_IN_TOP_LEFT, 20, 40);

    lv_obj_t *led_lbl = lv_label_create(power_bg, NULL);
    lv_label_set_static_text(led_lbl, "Bottom Green LED");
    lv_obj_align(led_lbl, power_bg, LV_ALIGN_IN_LEFT_MID, 20, 18);
    lv_obj_t *led_sw = lv_switch_create(power_bg, NULL);
    lv_obj_set_size(led_sw, 60, 16);
    lv_obj_set_event_cb(led_sw, led_event_handler);
    lv_obj_align(led_sw, power_bg, LV_ALIGN_IN_RIGHT_MID, -20, 18);

    lv_obj_t *vibr_lbl = lv_label_create(power_bg, NULL);
    lv_label_set_static_text(vibr_lbl, "Vibration Motor");
    lv_obj_align(vibr_lbl, power_bg, LV_ALIGN_IN_LEFT_MID, 20, 46);
    lv_obj_t *vibr_sw = lv_switch_create(power_bg, NULL);
    lv_obj_set_size(vibr_sw, 60, 16);
    lv_obj_set_event_cb(vibr_sw, vibration_event_handler);
    lv_obj_align(vibr_sw, power_bg, LV_ALIGN_IN_RIGHT_MID, -20, 46);

    lv_obj_t *brightness_lbl = lv_label_create(power_bg, NULL);
    lv_label_set_static_text(brightness_lbl, "Brightness");
    lv_obj_align(brightness_lbl, power_bg, LV_ALIGN_IN_LEFT_MID, 20, 74);
    lv_obj_t * brightness_slider = lv_slider_create(power_bg, NULL);
    lv_obj_set_width(brightness_slider, 100);
    lv_obj_align(brightness_slider, power_bg, LV_ALIGN_IN_RIGHT_MID, -20, 78);
    lv_obj_set_event_cb(brightness_slider, brightness_event_handler);
    lv_slider_set_value(brightness_slider, 80, LV_ANIM_OFF);
    lv_slider_set_range(brightness_slider, 30, 100);
    xSemaphoreGive(xGuiSemaphore);

    xTaskCreatePinnedToCore(battery_task, "batteryTask", configMINIMAL_STACK_SIZE * 2, (void *) core2forAWS_screen_obj, 0, &power_handle, 1);
}

static void brightness_event_handler(lv_obj_t *slider, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        uint16_t value = lv_slider_get_value(slider);

        Core2ForAWS_LCD_SetBrightness(value);
        ESP_LOGI(TAG, "Screen brightness: %d", value);
    }
}

static void led_event_handler(lv_obj_t *obj, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        uint8_t value = lv_switch_get_state(obj);

        Core2ForAWS_LED_Enable(value);
        ESP_LOGI(TAG, "LED state: %x", value);
    }
}

static void vibration_event_handler(lv_obj_t *obj, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        uint8_t value = lv_switch_get_state(obj);

        if(value == true)
            Core2ForAWS_Motor_SetStrength(50);
        else
            Core2ForAWS_Motor_SetStrength(0);
        
        ESP_LOGI(TAG, "Vibration motor state: %x", value);
    }
}

void battery_task(void *pvParameters){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t *battery_lbl = lv_label_create((lv_obj_t *)pvParameters, NULL);
    lv_label_set_text(battery_lbl, LV_SYMBOL_BATTERY_FULL);
    lv_label_set_recolor(battery_lbl, true);
    lv_label_set_align(battery_lbl, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(battery_lbl, (lv_obj_t *)pvParameters, LV_ALIGN_IN_TOP_RIGHT, -20, 10);
    lv_obj_t *charge_lbl = lv_label_create(battery_lbl, NULL);
    lv_label_set_recolor(charge_lbl, true);
    lv_label_set_text(charge_lbl, "");
    lv_obj_align(charge_lbl, battery_lbl, LV_ALIGN_CENTER, -4, 0);
    xSemaphoreGive(xGuiSemaphore);

    for(;;){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        float battery_voltage = Core2ForAWS_PMU_GetBatVolt();
        if(battery_voltage >= 4.100){
            lv_label_set_text(battery_lbl, "#0ab300 " LV_SYMBOL_BATTERY_FULL "#");
        } else if(battery_voltage >= 3.95){
            lv_label_set_text(battery_lbl, "#0ab300 " LV_SYMBOL_BATTERY_3 "#");
        } else if(battery_voltage >= 3.82){
            lv_label_set_text(battery_lbl, "#ff9900 " LV_SYMBOL_BATTERY_2 "#");
        } else if(battery_voltage >= 3.60){
            lv_label_set_text(battery_lbl, "#ff0000 " LV_SYMBOL_BATTERY_1 "#");
        } else{
            lv_label_set_text(battery_lbl, "#ff0000 " LV_SYMBOL_BATTERY_EMPTY "#");
        }

        if(Core2ForAWS_PMU_GetBatCurrent() >= 0.00){
            lv_label_set_text(charge_lbl, "#0000cc " LV_SYMBOL_CHARGE "#");
        } else{
            lv_label_set_text(charge_lbl, "");
        }
        xSemaphoreGive(xGuiSemaphore);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    vTaskDelete(NULL); // Should never get to here...
}