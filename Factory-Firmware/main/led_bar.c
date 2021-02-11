/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.0.0
 * led_bar.c
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
#include "led_bar.h"


static xSemaphoreHandle lock;

static uint8_t red = 255, green = 153, blue = 0;
static uint32_t color = 16750848; // Amazon Orange in Decimal

static const char *TAG = LED_BAR_TAB_NAME;

static void hue_event_handler(lv_obj_t *slider, lv_event_t event);
static void sat_event_handler(lv_obj_t *slider, lv_event_t event);
static void val_event_handler(lv_obj_t *slider, lv_event_t event);

void display_LED_bar_tab(lv_obj_t *tv){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_obj_t *led_bar_tab = lv_tabview_add_tab(tv, LED_BAR_TAB_NAME);

    /* Create the main body object and set background within the tab*/
    lv_obj_t *led_bar_bg = lv_obj_create(led_bar_tab, NULL);
    lv_obj_align(led_bar_bg, NULL, LV_ALIGN_IN_TOP_LEFT, 16, 36);
    lv_obj_set_size(led_bar_bg, 290, 190);
    lv_obj_set_click(led_bar_bg, false);
    
    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(85, 213, 255));
    lv_obj_add_style(led_bar_bg, LV_OBJ_PART_MAIN, &bg_style);

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_obj_t *tab_title_lbl = lv_label_create(led_bar_bg, NULL);
    lv_obj_add_style(tab_title_lbl, LV_OBJ_PART_MAIN, &title_style);
    lv_label_set_static_text(tab_title_lbl, "SK6812 LED Bars");
    lv_obj_align(tab_title_lbl, led_bar_bg, LV_ALIGN_IN_TOP_MID, 0, 10);

    /* Create the sensor information label object */
    lv_obj_t *body_lbl = lv_label_create(led_bar_bg, NULL);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_BREAK);
    lv_label_set_static_text(body_lbl, "The LED bars are ten SK6812 LEDs. You can control each LED's brightness and color.");
    lv_obj_set_width(body_lbl, 252);
    lv_obj_align(body_lbl, led_bar_bg, LV_ALIGN_IN_TOP_LEFT, 20, 40);

    lv_obj_t *LED_hue_lbl = lv_label_create(led_bar_bg, NULL);
    lv_label_set_static_text(LED_hue_lbl, "Red");
    lv_obj_align(LED_hue_lbl, led_bar_bg, LV_ALIGN_IN_LEFT_MID, 20, 20);
    lv_obj_t *hue_slider = lv_slider_create(led_bar_bg, NULL);
    lv_obj_set_width(hue_slider, 100);
    lv_obj_align(hue_slider, led_bar_bg, LV_ALIGN_IN_RIGHT_MID, -20, 20);
    lv_obj_set_event_cb(hue_slider, hue_event_handler);
    lv_slider_set_value(hue_slider, 255, LV_ANIM_OFF);
    lv_slider_set_range(hue_slider, 0, 255);

    lv_obj_t *LED_sat_lbl = lv_label_create(led_bar_bg, NULL);
    lv_label_set_static_text(LED_sat_lbl, "Green");
    lv_obj_align(LED_sat_lbl, led_bar_bg, LV_ALIGN_IN_LEFT_MID, 20, 45);
    lv_obj_t *sat_slider = lv_slider_create(led_bar_bg, NULL);
    lv_obj_set_width(sat_slider, 100);
    lv_obj_align(sat_slider, led_bar_bg, LV_ALIGN_IN_RIGHT_MID, -20, 45);
    lv_obj_set_event_cb(sat_slider, sat_event_handler);
    lv_slider_set_value(sat_slider, 153, LV_ANIM_OFF);
    lv_slider_set_range(sat_slider, 0, 255);

    lv_obj_t *LED_val_lbl = lv_label_create(led_bar_bg, NULL);
    lv_label_set_static_text(LED_val_lbl, "Blue");
    lv_obj_align(LED_val_lbl, led_bar_bg, LV_ALIGN_IN_LEFT_MID, 20, 70);
    lv_obj_t *val_slider = lv_slider_create(led_bar_bg, NULL);
    lv_obj_set_width(val_slider, 100);
    lv_obj_align(val_slider, led_bar_bg, LV_ALIGN_IN_RIGHT_MID, -20, 70);
    lv_obj_set_event_cb(val_slider, val_event_handler);
    lv_slider_set_value(val_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(val_slider, 0, 255);

    xSemaphoreGive(xGuiSemaphore);

    xTaskCreatePinnedToCore(sk6812_task, "sk6812Task", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL, 1);
}

static void update_color(){
    xSemaphoreTake(lock, portMAX_DELAY);
    color = (red << 16) + (green << 8) + (blue);
    xSemaphoreGive(lock);
}

static void hue_event_handler(lv_obj_t *slider, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        red = lv_slider_get_value(slider);
        update_color();
        ESP_LOGI(TAG, "Red: %d Color: %d", red, color);
    }
}

static void sat_event_handler(lv_obj_t *slider, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        green = lv_slider_get_value(slider);
        update_color();
        ESP_LOGI(TAG, "Green: %d", green);
    }
}

static void val_event_handler(lv_obj_t *slider, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        blue = lv_slider_get_value(slider);
        update_color();
        ESP_LOGI(TAG, "Blue: %d", blue);
    }
}

void sk6812_task(void *pvParameters) {
    lock = xSemaphoreCreateMutex();
    while (1) {
        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();

        for (uint8_t i = 0; i < 10; i++) {
            xSemaphoreTake(lock, portMAX_DELAY);
            Core2ForAWS_Sk6812_SetColor(i, color);
            xSemaphoreGive(lock);
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(70));
        }

        for (uint8_t i = 0; i < 10; i++) {
            Core2ForAWS_Sk6812_SetColor(i, 0x000000);
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(70));
        }

        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x232f3e);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xffffff);
        Core2ForAWS_Sk6812_Show();

        for (uint8_t i = 40; i > 0; i--) {
            Core2ForAWS_Sk6812_SetBrightness(i);
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(25));
        }

        Core2ForAWS_Sk6812_SetBrightness(20);
    }
    vTaskDelete(NULL); // Should never get to here...
}