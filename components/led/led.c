#include "led.h"

static const char* TAG = "LED";

const colorValues_t color_values[] = {
    [RED]   = {10,  0,  0},
    [GREEN] = { 0, 10,  0},
    [BLUE]  = { 0,  0, 10},
    [WHITE] = {10, 10, 10}
};

/* Private variables */
#ifdef CONFIG_BLINK_LED_STRIP
static led_strip_handle_t led_strip;
#endif

/* Public functions */
#ifdef CONFIG_BLINK_LED_STRIP

void fill_led_strip(uint8_t r, uint8_t g, uint8_t b) {
    led_strip_clear(led_strip);

    for (uint8_t i = 0; i < 25; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }

    led_strip_refresh(led_strip);
}

void fill_led_strip_with_color(color_t color) {
    ESP_LOGV("LED_STRIP", "Color for filling LED strip is %d", color);
    fill_led_strip(color_values[color].r, color_values[color].g, color_values[color].b);
}

void led_init(void) {
    ESP_LOGI(TAG, "project configured with addressable led strip!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip)
    );
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(
        led_strip_new_spi_device(&strip_config, &spi_config, &led_strip)
    );
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LEDs initialized\n");
}

#elif CONFIG_BLINK_LED_GPIO

void led_init(void) {
    ESP_LOGI(TAG, "project configured with gpio led!");
    gpio_reset_pin(CONFIG_BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "LEDs initialized\n");
}

#else
#error "unsupported LED type"
#endif
