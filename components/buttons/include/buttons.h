#ifndef BUTTONS_H
#define BUTTONS_H

#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "esp_timer.h"

#if CONFIG_POTENTIOMETER_ACTIVE
    #define BUTTON_GPIO CONFIG_BUTTON_GPIO
#else
    #define BUTTON_GPIO_LEFT CONFIG_BUTTON_GPIO_LEFT
    #define BUTTON_GPIO_RIGHT CONFIG_BUTTON_GPIO_RIGHT
#endif

#if CONFIG_ENABLE_GPIO_PULLDOWN
    #define BUTTON_PRESSED        1
    #define BUTTON_RELEASED       0
#else
    #define BUTTON_PRESSED        0
    #define BUTTON_RELEASED       1
#endif

typedef struct {
    uint8_t gpio_num;   // GPIO number of the button
    uint8_t event;      // 0 = pressed, 1 = released
    uint64_t timestamp; // Timestamp in microseconds
} button_event_t;

extern QueueHandle_t button_queue;

void buttons_init(void);
void buttons_cleanup(void);

#endif /* BUTTONS_H */