#include "buttons.h"

static const char *TAG = "BUTTONS";

QueueHandle_t button_queue = NULL;

static void create_buttonQueue() {
    button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return;
    }
    ESP_LOGI(TAG, "Created button queue");
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    button_event_t event;

    event.gpio_num = gpio_num;
    event.event = gpio_get_level(gpio_num);
    event.timestamp = esp_timer_get_time();

    xQueueSendFromISR(button_queue, &event, NULL);
}

void buttons_init() {
    gpio_config_t gpioConfigIn = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    #if CONFIG_POTENTIOMETER_ACTIVE
        gpioConfigIn.pin_bit_mask = (1 << BUTTON_GPIO);
        ESP_LOGD(TAG, "Configured for single button on GPIO %d", BUTTON_GPIO);
    #else
        gpioConfigIn.pin_bit_mask = (1 << BUTTON_GPIO_LEFT) | (1 << BUTTON_GPIO_RIGHT);
        ESP_LOGD(TAG, "Configured for both buttons on GPIO %d and %d", BUTTON_GPIO_LEFT, BUTTON_GPIO_RIGHT);
    #endif

    #if CONFIG_ENABLE_GPIO_PULLDOWN
        gpioConfigIn.pull_up_en = GPIO_PULLUP_DISABLE;
        gpioConfigIn.pull_down_en = GPIO_PULLDOWN_ENABLE;
    #endif
    gpio_config(&gpioConfigIn);

    gpio_install_isr_service(0);
    esp_err_t err;

    #if CONFIG_POTENTIOMETER_ACTIVE
        err = gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR handler for BUTTON_GPIO: %s", esp_err_to_name(err));
        }
    #else
        err = gpio_isr_handler_add(BUTTON_GPIO_LEFT, gpio_isr_handler, (void*) BUTTON_GPIO_LEFT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR handler for BUTTON_GPIO_LEFT: %s", esp_err_to_name(err));
        }

        err = gpio_isr_handler_add(BUTTON_GPIO_RIGHT, gpio_isr_handler, (void*) BUTTON_GPIO_RIGHT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR handler for BUTTON_GPIO_RIGHT: %s", esp_err_to_name(err));
        }
    #endif

    create_buttonQueue();

    ESP_LOGI(TAG, "Buttons initialized\n"); 
}

void buttons_cleanup() {
    #if CONFIG_POTENTIOMETER_ACTIVE
        gpio_isr_handler_remove(BUTTON_GPIO);
    #else
        gpio_isr_handler_remove(BUTTON_GPIO_LEFT);
        gpio_isr_handler_remove(BUTTON_GPIO_RIGHT);
    #endif

    gpio_uninstall_isr_service();
}