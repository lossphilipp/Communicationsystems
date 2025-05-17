#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_PERIOD CONFIG_BLINK_PERIOD
#define BUTTON_GPIO_LEFT CONFIG_BUTTON_GPIO_LEFT
#define BUTTON_GPIO_RIGHT CONFIG_BUTTON_GPIO_RIGHT

#define TASKS_STACKSIZE        2048
#define TASKS_PRIORITY            3
#define BUTTON_HOLD_TIMEOUT  500000 // 0.5 second

#if CONFIG_ENABLE_GPIO_PULLDOWN
    #define BUTTON_PRESSED        1
    #define BUTTON_RELEASED       0
#else
    #define BUTTON_PRESSED        0
    #define BUTTON_RELEASED       1
#endif

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} colorValues_t;

typedef enum {
    RED,
    GREEN,
    BLUE,
    WHITE,
    COLOR_COUNT
} color_t;

const char* color_to_string(color_t color) {
    switch (color) {
        case RED:   return "RED";
        case GREEN: return "GREEN";
        case BLUE:  return "BLUE";
        case WHITE: return "WHITE";
        default:    return "UNKNOWN";
    }
}

color_t current_color = RED;
uint8_t led_brightness = 10;

const colorValues_t color_values[] = {
    [RED]   = {1, 0, 0},
    [GREEN] = {0, 1, 0},
    [BLUE]  = {0, 0, 1},
    [WHITE] = {1, 1, 1}
};

#ifdef CONFIG_BLINK_LED_STRIP

/* #################### LED #################### */

static led_strip_handle_t led_strip;

static void configure_led(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 25,
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

void fill_led_strip(uint8_t r, uint8_t g, uint8_t b) {
    led_strip_clear(led_strip);

    // ToDo: Optimize this
    if (r == 1) {
        r = led_brightness;
    }
    if (g == 1) {
        g = led_brightness;
    }
    if (b == 1) {
        b = led_brightness;
    }

    for (uint8_t i = 0; i < 25; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }

    led_strip_refresh(led_strip);
}

void fill_led_strip_with_color(color_t color) {
    ESP_LOGV("LED_STRIP", "Color for filling LED strip is %s", color_to_string(color));
    fill_led_strip(color_values[color].r, color_values[color].g, color_values[color].b);
}

void clear_led_strip() {
    ESP_LOGV("LED_STRIP", "Clearing led strip");
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

void change_color() {
    current_color = (current_color + 1) % COLOR_COUNT;
}

const uint8_t rolling_pattern[8][5][5] = {
    {
        {0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    },
    {
        {0, 0, 0, 0, 1},
        {0, 0, 0, 1, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    },
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 1, 1, 1},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    },
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0},
        {0, 0, 0, 0, 1}
    },
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 1, 0, 0}
    },
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 1, 0, 0, 0},
        {1, 0, 0, 0, 0}
    },
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {1, 1, 1, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    },
    {
        {1, 0, 0, 0, 0},
        {0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}
    }
};

void draw_roll_animation(){
    for (size_t i = 0; i < 3; i++) {
        for (uint8_t i = 0; i < 8; i++) {
            led_strip_clear(led_strip);

            uint8_t current_patterns[5][5];
            memcpy(current_patterns, rolling_pattern[i], sizeof(uint8_t) * 5 * 5);

            for (uint8_t row = 0; row < 5; row++) {
                for (uint8_t col = 0; col < 5; col++) {
                    if (current_patterns[row][col] == 1) {
                        uint8_t pixel_index = row * 5 + col;
                        led_strip_set_pixel(led_strip, pixel_index, 20, 20, 20);
                    }
                }
            }

            led_strip_refresh(led_strip);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
    fill_led_strip_with_color(WHITE);
}

/* #################### Buttons #################### */

static QueueHandle_t button_queue;

typedef struct {
    uint8_t gpio_num;   // GPIO number of the button
    uint8_t event;      // 0 = pressed, 1 = released
    uint64_t timestamp; // Timestamp in microseconds
} button_event_t;

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

/* #################### Tasks #################### */

TaskHandle_t gBlinkTask_handle = NULL;

void blink_task(void *pBlinkPeriod_ms) {
    bool lightOn = false;
    while (true) {
        ESP_LOGD("BLINK_TASK", "Switching light %s", lightOn ? "on" : "off");

        if (lightOn) {
            fill_led_strip_with_color(current_color);
        } else {
            clear_led_strip();
        }
        vTaskDelay(*(uint32_t*)pBlinkPeriod_ms / portTICK_PERIOD_MS);
        lightOn = !lightOn;
    }
}

TaskHandle_t gButtonTask_handle = NULL;

void handle_shortPress(uint8_t gpio_num) {
    if (gpio_num == BUTTON_GPIO_LEFT) {
        led_brightness = (led_brightness + 10);
        if (led_brightness >= 75) {
            led_brightness = 10;
        }
        ESP_LOGD("BUTTON_TASK", "Brightness changed to %d", led_brightness);
    } else if (gpio_num == BUTTON_GPIO_RIGHT) {
        if (gBlinkTask_handle != NULL) {
            eTaskState task_state = eTaskGetState(gBlinkTask_handle);
            if (task_state == eSuspended) {
                vTaskResume(gBlinkTask_handle);
            } else {
                vTaskSuspend(gBlinkTask_handle);
            }
            ESP_LOGD("BUTTON_TASK", "Blink task %s", task_state == eSuspended ? "resumed" : "suspended");
        }
    }
}

void handle_longPress(uint8_t gpio_num) {
    if (gpio_num == BUTTON_GPIO_LEFT) {
        current_color = (current_color + 1) % 4;
        ESP_LOGD("BUTTON_TASK", "Color changed to %s", color_to_string(current_color));
    } else if (gpio_num == BUTTON_GPIO_RIGHT) {
        ESP_LOGD("BUTTON_TASK", "Drawing roll animation");

        if (gBlinkTask_handle != NULL && eTaskGetState(gBlinkTask_handle) == eRunning) {
            vTaskSuspend(gBlinkTask_handle);
            draw_roll_animation();
            vTaskResume(gBlinkTask_handle);
        } else {
            draw_roll_animation();
        }
    }
}

void button_task(void *arguments) {
    button_event_t event;
    uint64_t press_start_time = 0;

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
                     event.gpio_num, button_name, event.event == 0 ? "pressed" : "released", event.timestamp);

            if (event.event == BUTTON_PRESSED) {
                press_start_time = event.timestamp;
            } else if (event.event == BUTTON_RELEASED) {
                uint64_t press_duration = event.timestamp - press_start_time;
                ESP_LOGV("BUTTON_TASK", "Button %d pressed for %llu ms", event.gpio_num, press_duration / 1000);

                if (press_duration < BUTTON_HOLD_TIMEOUT) {
                    handle_shortPress(event.gpio_num);
                } else {
                    handle_longPress(event.gpio_num);
                }
            }
        }
    }
}

/* #################### Program #################### */

void app_main(void)
{
    configure_led();
    ESP_LOGI("CONFIGURATION", "LEDs configured");

    configure_buttons();

    create_buttonQueue();

    static uint32_t blinkPeriod_ms = BLINK_PERIOD;

    xTaskCreate(button_task, "button_task", TASKS_STACKSIZE, NULL, TASKS_PRIORITY, &gButtonTask_handle);
    xTaskCreate(blink_task, "blink_task", TASKS_STACKSIZE, &blinkPeriod_ms, TASKS_PRIORITY, &gBlinkTask_handle);

    ESP_LOGI("CONFIGURATION", "Tasks created, start program...");

    // Prevent app_main from exiting
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }

    cleanup_button_config();
}