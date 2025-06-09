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
// JSON schema:
// {
//   "color": {
//     "r": 255,
//     "g": 180,
//     "b": 200
//   },
//   "state": "ON"
// }

void mqtt_led_control_callback(const char *topic, const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) return;

    // Handle color
    cJSON *color = cJSON_GetObjectItem(root, "color");
    int led_r = 255, led_g = 255, led_b = 255;
    if (color) {
        cJSON *r = cJSON_GetObjectItem(color, "r");
        cJSON *g = cJSON_GetObjectItem(color, "g");
        cJSON *b = cJSON_GetObjectItem(color, "b");
        if (r && g && b) {
            led_r = r->valueint;
            led_g = g->valueint;
            led_b = b->valueint;
            fill_led_strip(led_r, led_g, led_b);
        }
    }

    // Handle state (optional, for ON/OFF)
    cJSON *state = cJSON_GetObjectItem(root, "state");
    char *state_str = NULL;
    if (state && cJSON_IsString(state)) {
        state_str = strdup(state->valuestring); // Copy the string
        if (strcmp(state_str, "OFF") == 0) {
            fill_led_strip(0, 0, 0); // Turn off the LED
        }
    } else {
        state_str = strdup("ON");
    }

    cJSON_Delete(root);

    // Publish new state back to MQTT
    cJSON *state_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(state_obj, "state", state_str);
    cJSON *color_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(color_obj, "r", led_r);
    cJSON_AddNumberToObject(color_obj, "g", led_g);
    cJSON_AddNumberToObject(color_obj, "b", led_b);
    cJSON_AddItemToObject(state_obj, "color", color_obj);

    char *led_state_str = cJSON_PrintUnformatted(state_obj);
    mqtt_sendpayload(MQTT_TOPIC_LED_STATE, (uint8_t*)led_state_str, strlen(led_state_str));

    // Clean up
    free(led_state_str);
    cJSON_Delete(state_obj);
    free(state_str);
}

// ########## button ##########
// JSON schema:
// {
//   "gpio": 255,
//   "event": "press" | "release"
// }

void publish_button_event(uint8_t gpio_num, uint8_t event) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "gpio", gpio_num);
    cJSON_AddStringToObject(root, "event", event == BUTTON_PRESSED ? "press" : "release");
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
// JSON schema:
// {
//   "value": 255
// }

void publish_potentiometer_event(uint8_t brightness) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "value", brightness);
    char *json_str = cJSON_PrintUnformatted(root);
    mqtt_sendpayload(MQTT_TOPIC_POTENTIOMETER, (uint8_t*)json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
}

void potentiometer_task(void *arg) {
    while (true) {
        uint8_t brightness = potentiometer_read_uint8();
        publish_potentiometer_event(brightness);
        vTaskDelay(pdMS_TO_TICKS(5000)); // ToDo: Set back to 200 before submission
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

    // ToDo: Implement in mqtt_impl
    // mqtt_subscribe(MQTT_TOPIC_LED_SET, mqtt_led_control_callback);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    buttons_cleanup();
}
