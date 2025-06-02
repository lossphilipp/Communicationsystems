#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "led.h"
#include "wifi_station.h"
#include "packet_sender.h"
#include "mqtt_impl.h"
#include "buttons.h"

#define TASKS_STACKSIZE        2048
#define TASKS_PRIORITY            3

TaskHandle_t gButtonTask_handle = NULL;

static uint64_t custom_htonll(uint64_t value) {
    // Convert 64-bit integer to network byte order
    uint32_t high_part = htonl((uint32_t)(value >> 32));
    uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFF));
    return ((uint64_t)low_part << 32) | high_part;
}

void build_packet(uint8_t gpio_num, uint8_t event, uint64_t timestamp, uint8_t *packet, size_t *packet_size) {
    // ToDo: Packet still not optimized
    packet[0] = gpio_num; // GPIO number (1 byte)
    packet[1] = event;    // Event (1 byte)
    uint64_t timestamp_network = custom_htonll(timestamp);

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
            #elif CONFIG_TRANSPORT_MQTT
            sendpacket_sendMQTT((uint8_t*)buf, packet_size);
            #endif
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

    led_init();
    buttons_init();
    staticwifi_init();
    mqtt_init();

    xTaskCreate(button_task, "button_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gButtonTask_handle);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    buttons_cleanup();
}
