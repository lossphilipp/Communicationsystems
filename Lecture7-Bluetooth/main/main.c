#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_timer.h"

#include "ble_device.h"

#define BUTTON_GPIO_LEFT CONFIG_BUTTON_GPIO_LEFT
#define BUTTON_GPIO_RIGHT CONFIG_BUTTON_GPIO_RIGHT

#define TASKS_STACKSIZE        2048
#define TASKS_PRIORITY            3

#if CONFIG_ENABLE_GPIO_PULLDOWN
    #define BUTTON_PRESSED        1
    #define BUTTON_RELEASED       0
#else
    #define BUTTON_PRESSED        0
    #define BUTTON_RELEASED       1
#endif

static QueueHandle_t button_queue;
TaskHandle_t gButtonTask_handle = NULL;

uint8_t gLeftButtonstatus = BUTTON_RELEASED;
uint8_t gRightButtonstatus = BUTTON_RELEASED;

typedef struct {
    uint8_t gpio_num;   // GPIO number of the button
    uint8_t event;      // 0 = pressed, 1 = released
    uint64_t timestamp; // Timestamp in microseconds
} button_event_t;

/* #################### Buttons #################### */
void create_buttonQueue() {
    button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_queue == NULL) {
        ESP_LOGE("CONFIGURATION", "Failed to create button queue");
        return;
    }
    ESP_LOGI("CONFIGURATION", "Created button queue");
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    button_event_t event;

    event.gpio_num = gpio_num;
    event.event = gpio_get_level(gpio_num);
    event.timestamp = esp_timer_get_time();

    xQueueSendFromISR(button_queue, &event, NULL);
}

static void configure_buttons() {
    gpio_config_t gpioConfigIn = {
        .pin_bit_mask = (1 << BUTTON_GPIO_LEFT) | (1 << BUTTON_GPIO_RIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    #if CONFIG_ENABLE_GPIO_PULLDOWN
        gpioConfigIn.pull_up_en = GPIO_PULLUP_DISABLE;
        gpioConfigIn.pull_down_en = GPIO_PULLDOWN_ENABLE;
    #endif
    gpio_config(&gpioConfigIn);

    gpio_install_isr_service(0);
    esp_err_t err;

    err = gpio_isr_handler_add(BUTTON_GPIO_LEFT, gpio_isr_handler, (void*) BUTTON_GPIO_LEFT);
    if (err != ESP_OK) {
        ESP_LOGE("CONFIGURATION", "Failed to add ISR handler for BUTTON_GPIO_LEFT: %s", esp_err_to_name(err));
    }

    err = gpio_isr_handler_add(BUTTON_GPIO_RIGHT, gpio_isr_handler, (void*) BUTTON_GPIO_RIGHT);
    if (err != ESP_OK) {
        ESP_LOGE("CONFIGURATION", "Failed to add ISR handler for BUTTON_GPIO_RIGHT: %s", esp_err_to_name(err));
    }

    ESP_LOGI("CONFIGURATION", "Buttons configured"); 
}

static void cleanup_button_config() {
    gpio_isr_handler_remove(BUTTON_GPIO_LEFT);
    gpio_isr_handler_remove(BUTTON_GPIO_RIGHT);
    gpio_uninstall_isr_service();
}

void button_task(void *arguments) {
    button_event_t event;

    while (true) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            if (event.gpio_num == BUTTON_GPIO_LEFT) {
                gLeftButtonstatus = event.event; // For Bluetooth task
            } else if (event.gpio_num == BUTTON_GPIO_RIGHT) {
                gRightButtonstatus = event.event; // For Bluetooth task
            } else {
                ESP_LOGW("BUTTON_TASK", "Unknown GPIO: %d", event.gpio_num);
            }

            uint16_t bothButtons = gLeftButtonstatus | (gRightButtonstatus << 8);
            ESP_LOGI("BLUETOOTH_TASK", "Sending button status: %d (Left: %d, Right: %d)", bothButtons, gLeftButtonstatus, gRightButtonstatus);
            ble_device_notify(bothButtons);
        }
    }
}

void app_main(void)
{
    configure_buttons();
    create_buttonQueue();

    ble_device_init();
    ble_device_start();

    xTaskCreate(button_task, "button_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gButtonTask_handle);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    cleanup_button_config();
}
