#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "cJSON.h"

#include "led.h"
#include "wifi_station.h"
#include "packet_sender.h"
#include "mqtt_impl.h"
#include "buttons.h"
#include "potentiometer.h"

#define TASKS_STACKSIZE        2048
#define TASKS_PRIORITY            3

#define MQTT_TOPIC_BUTTON           "button"
#define MQTT_TOPIC_POTENTIOMETER    "potentiometer"
#define MQTT_TOPIC_LED_SET          "led/set"
#define MQTT_TOPIC_LED_STATE        "led/state"

TaskHandle_t gButtonTask_handle = NULL;
TaskHandle_t gPotentiometerTask_handle = NULL;

// ########## led ##########

// Example implementation by Copilot
// void mqtt_led_control_callback(const char *topic, const char *payload) {
//     cJSON *root = cJSON_Parse(payload);
//     if (!root) return;

//     // Handle brightness
//     cJSON *brightness = cJSON_GetObjectItem(root, "brightness");
//     if (brightness) {
//         led_brightness = brightness->valueint;
//         led_set_brightness(led_brightness); // Implement this in led.c
//     }

//     // Handle color
//     cJSON *color = cJSON_GetObjectItem(root, "color");
//     if (color) {
//         cJSON *r = cJSON_GetObjectItem(color, "r");
//         cJSON *g = cJSON_GetObjectItem(color, "g");
//         cJSON *b = cJSON_GetObjectItem(color, "b");
//         if (r && g && b) {
//             led_r = r->valueint;
//             led_g = g->valueint;
//             led_b = b->valueint;
//             led_set_rgb(led_r, led_g, led_b); // Implement this in led.c
//         }
//     }

//     cJSON_Delete(root);

//     // Optionally, publish new state back to MQTT
//     cJSON *state = cJSON_CreateObject();
//     cJSON_AddStringToObject(state, "state", "ON");
//     cJSON_AddNumberToObject(state, "brightness", led_brightness);
//     cJSON *color_obj = cJSON_CreateObject();
//     cJSON_AddNumberToObject(color_obj, "r", led_r);
//     cJSON_AddNumberToObject(color_obj, "g", led_g);
//     cJSON_AddNumberToObject(color_obj, "b", led_b);
//     cJSON_AddItemToObject(state, "color", color_obj);

//     char *state_str = cJSON_PrintUnformatted(state);
//     mqtt_sendpayload(MQTT_TOPIC_LED_STATE, (uint8_t*)state_str, strlen(state_str));
//     free(state_str);
//     cJSON_Delete(state);
// }

// ########## button ##########

void publish_button_event(uint8_t gpio_num, uint8_t event) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "gpio", gpio_num);
    cJSON_AddStringToObject(root, "event", event == BUTTON_PRESSED ? "pressed" : "released"); // Todo: Use enum or integer constants
    char *json_str = cJSON_PrintUnformatted(root);
    mqtt_sendpayload(MQTT_TOPIC_BUTTON, (uint8_t*)json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
}

void button_task(void *arguments) {
    button_event_t event;

    while (true) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            if (event.gpio_num == BUTTON_GPIO) {
                publish_button_event(event.gpio_num, event.event);
            } else {
                ESP_LOGW("BUTTON_TASK", "Unknown GPIO: %d", event.gpio_num);
            }
        }
    }
}

// ########## potentiometer ##########

void publish_potentiometer_event(uint8_t percent) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "value", percent);
    char *json_str = cJSON_PrintUnformatted(root);
    mqtt_sendpayload(MQTT_TOPIC_POTENTIOMETER, (uint8_t*)json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
}

void potentiometer_task(void *arg) {
    while (true) {
        uint8_t percent = potentiometer_read_percentage();
        publish_potentiometer_event(percent);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ########## main ##########

void init_nvs(void) {
    // Initialize NVS partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_init()); // Retry nvs_flash_init
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    init_nvs();

    led_init();
    buttons_init();
    potentiometer_init();
    staticwifi_init();
    mqtt_init();

    xTaskCreate(button_task, "button_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gButtonTask_handle);
    xTaskCreate(potentiometer_task, "potentiometer_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gPotentiometerTask_handle);

    // mqtt_subscribe(MQTT_TOPIC_LED_SET, mqtt_led_control_callback);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    buttons_cleanup();
}
