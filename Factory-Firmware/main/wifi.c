/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.0.0
 * wifi.c
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
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"

#include "core2forAWS.h"
#include "lvgl/lvgl.h"
#include "wifi.h"

#define DEFAULT_SCAN_LIST_SIZE 6

static const char *TAG = WIFI_TAB_NAME;

static void event_handler(lv_obj_t * obj, lv_event_t event);
static void wifi_scan_task(void *pvParameters);

void display_wifi_tab(lv_obj_t *tv){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_obj_t *wifi_tab = lv_tabview_add_tab(tv, WIFI_TAB_NAME);

    /* Create the main body object and set background within the tab*/
    lv_obj_t *wifi_bg = lv_obj_create(wifi_tab, NULL);
    lv_obj_align(wifi_bg, NULL, LV_ALIGN_IN_TOP_LEFT, 16, 36);
    lv_obj_set_size(wifi_bg, 290, 190);
    lv_obj_set_click(wifi_bg, false);
    
    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(180, 230, 34));
    lv_obj_add_style(wifi_bg, LV_OBJ_PART_MAIN, &bg_style);

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_obj_t *tab_title_lbl = lv_label_create(wifi_bg, NULL);
    lv_obj_add_style(tab_title_lbl, LV_OBJ_PART_MAIN, &title_style);
    lv_label_set_static_text(tab_title_lbl, "Wi-Fi Scan (2.4GHz)");
    lv_obj_align(tab_title_lbl, wifi_bg, LV_ALIGN_IN_TOP_MID, 0, 10);

    /* Create the sensor information label object */
    lv_obj_t *body_lbl = lv_label_create(wifi_bg, NULL);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_BREAK);
    lv_label_set_static_text(body_lbl, "Built-in 2.4GHz Wi-Fi and Bluetooth shared radio.");
    lv_obj_set_width(body_lbl, 252);
    lv_obj_align(body_lbl, wifi_bg, LV_ALIGN_IN_TOP_LEFT, 20, 40);
    
    /*Create a list*/
    lv_obj_t *list1 = lv_list_create(wifi_bg, NULL);
    lv_obj_set_size(list1, 260, 90);
    lv_obj_align(list1, wifi_bg, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
    lv_list_set_edge_flash(list1, true);

    xSemaphoreGive(xGuiSemaphore);
    xTaskCreatePinnedToCore(wifi_scan_task, "WiFiScanTask", configMINIMAL_STACK_SIZE * 4, (void *)list1, 1, &wifi_handle, 1);
}

static void event_handler(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_CLICKED) {
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
        xSemaphoreGive(xGuiSemaphore);
    }
}

static void wifi_scan_task(void *pvParameters){
    vTaskSuspend(NULL);

    /*Add buttons to the list*/
    lv_obj_t *list_btn;
    
    /* Initialize Wi-Fi as sta and set scan method */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    while(1){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        lv_list_clean((lv_obj_t *)pvParameters);
        xSemaphoreGive(xGuiSemaphore);

        esp_wifi_scan_start(NULL, true);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
        for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
            xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
            list_btn = lv_list_add_btn((lv_obj_t *)pvParameters, LV_SYMBOL_WIFI, (char *)ap_info[i].ssid );
            lv_obj_set_event_cb(list_btn, event_handler);
            xSemaphoreGive(xGuiSemaphore);

            ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
            ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
        }
        vTaskSuspend(NULL);
    }
    
    vTaskDelete(NULL); // Should never get to here...
}