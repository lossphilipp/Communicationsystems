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

#include "wifi_station.h"
#include "packet_sender.h"
#include "mqtt_impl.h"

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

static uint64_t htonll(uint64_t value) {
    // Convert 64-bit integer to network byte order
    uint32_t high_part = htonl((uint32_t)(value >> 32));
    uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFF));
    return ((uint64_t)low_part << 32) | high_part;
}

void build_packet(uint8_t gpio_num, uint8_t event, uint64_t timestamp, uint8_t *packet, size_t *packet_size) {
    // ToDo: Packet still not optimized
    packet[0] = gpio_num; // GPIO number (1 byte)
    packet[1] = event;    // Event (1 byte)
    uint64_t timestamp_network = htonll(timestamp);

    memcpy(&packet[2], &timestamp_network, sizeof(timestamp_network));
    *packet_size = 2 + sizeof(timestamp_network);
}

void button_task(void *arguments) {
    button_event_t event;

    while (true) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            const char *button_name;
            if (event.gpio_num == BUTTON_GPIO_LEFT) {
                button_name = "LEFT";
            } else if (event.gpio_num == BUTTON_GPIO_RIGHT) {
                button_name = "RIGHT";
            } else {
                ESP_LOGW("BUTTON_TASK", "Unknown GPIO: %d", event.gpio_num);
                button_name = "UNKNOWN";
            }

            ESP_LOGI("BUTTON_TASK", "Button event received:\nGPIO=%d (%s), Event=%s, Timestamp=%llu", 
                event.gpio_num, button_name, event.event == 0 ? "pressed" : "released", event.timestamp
            );

            uint8_t buf[10];
            size_t packet_size;
            build_packet(event.gpio_num, event.event, event.timestamp, buf, &packet_size);

            #if CONFIG_TRANSPORT_UDP
            packetsender_sendUDP(CONFIG_IPV4_ADDR, CONFIG_PORT, (uint8_t*)buf, packet_size);
            #elif CONFIG_TRANSPORT_TCP
            packetsender_sendTCP(CONFIG_IPV4_ADDR, CONFIG_PORT, (uint8_t*)buf, packet_size);
            #endif
            sendpacket_sendMQTT((uint8_t*)buf, packet_size);
        }
    }
}

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

    configure_buttons();
    create_buttonQueue();

    staticwifi_init();

    xTaskCreate(button_task, "button_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gButtonTask_handle);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    cleanup_button_config();
}
