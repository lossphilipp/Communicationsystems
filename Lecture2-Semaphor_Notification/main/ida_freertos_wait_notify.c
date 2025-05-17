#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#define BUTTON_GPIO         GPIO_NUM_2
#define LED_GPIO            GPIO_NUM_8

#define USE_SEMAPHORE       CONFIG_USE_SEMAPHORE

#define EVT_BUTTON_PRESSED  0x00000001
#define EVT_BUTTON_RELEASED 0x00000002
#define EVT_SOME1           0x00000004
#define EVT_SOME2           0x00000008
#define EVT_SOME3           0x00000010
#define EVT_SOME4           0x00000020

static led_strip_handle_t led_strip;
TaskHandle_t led_task_handle = NULL;

static const char *TAG = "TASK_PROJECT";

static void configure_led(void) {
    ESP_LOGI(TAG, "Configuring LEDs...");
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 25,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000 // 10 MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

static void configure_button(void) {
    ESP_LOGI(TAG, "Initializing button");
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&button_config);
    //gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
	//gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);
}

void button_task(void) {
    uint32_t btnActive = 0;
    while (true) {
		if (gpio_get_level(BUTTON_GPIO) == 0) { // pressed
			if (++btnActive == 2) { // pressed long enough
                #if USE_SEMAPHORE == 1
                #else
				xTaskNotify(led_task_handle, EVT_BUTTON_PRESSED, eSetBits); // eNoAction like xTaskNotifyGive, eSetBits
                #endif
			}
		} else {
			btnActive = 0;
		}
		vTaskDelay(50 / portTICK_PERIOD_MS); // one system tick default 10 ms
        // wenn zu kurz, kommt man nicht in idle task und der watchdog
        // kriegt keine info mehr und resettet das system
	}
}

void led_task(void) {
    bool button_pressed = false;
    while (true) {
        // using notify take since we are not interested in a value, only in the notification
        // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // for eNoAction use ulTaskNotifyTake
        // for eSetBits use ulTaskNotifyWait we can check for specific bit
        uint32_t value;
        ulTaskNotifyWait(0xFFFFFFFF, 0, &value, portMAX_DELAY); 
        
        // if (value & (EVT_BUTTON_PRESSED | EVT_BUTTON_RELEASED)) { // pressed or released
        if (value & EVT_BUTTON_PRESSED) {
            if (!button_pressed) {
                led_strip_set_pixel(led_strip, 0, 10, 10, 10);
                led_strip_refresh(led_strip);
                button_pressed = true;
            } else {
                led_strip_clear(led_strip);
                button_pressed = false;
            }
        } else if (value & EVT_SOME1) {
            // do something
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "App start: 2 Tasks, using FreeRTOS wait and notify.");

    // Initialize LEDs and button
    configure_led();
    configure_button();

    // Create task for reading button and blinking led
    xTaskCreate(&led_task, "led_task", 4096, NULL, 5, &led_task_handle);
    xTaskCreate(&button_task, "button_task", 4096, NULL, 5, NULL);
}