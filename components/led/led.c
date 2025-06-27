#include "led.h"

static const char* TAG = "LED";

const colorValues_t color_values[] = {
    [RED]   = {10,  0,  0},
    [GREEN] = { 0, 10,  0},
    [BLUE]  = { 0,  0, 10},
    [WHITE] = {10, 10, 10}
};

current_led_state_t current_led_state = {
    .color = {0, 0, 0},
    .brightness = 255,
    .state = false
};

#ifdef CONFIG_BLINK_LED_STRIP
/* Private variables */
led_strip_handle_t led_strip;

/* Public functions */
void adjust_led_brightness(uint8_t brightness) {
    ESP_LOGI(TAG, "Adjusting LED brightness to %d", brightness);
    current_led_state.brightness = brightness;
    fill_led_strip_with_colorValues(current_led_state.color);
}

void fill_led_strip(uint8_t r, uint8_t g, uint8_t b) {
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return;
    }

    if (!current_led_state.state)
    {
        ESP_LOGW(TAG, "LED strip is off, cannot fill with color");
        return;
    }

    ESP_LOGV(TAG, "Original color: R=%d, G=%d, B=%d\n", r, g, b);

    // Set current color to the new values
    current_led_state.color.r = r;
    current_led_state.color.g = g;
    current_led_state.color.b = b;

    // Adjust the color values based on the current brightness
    r = (r * current_led_state.brightness) / 255;
    g = (g * current_led_state.brightness) / 255;
    b = (b * current_led_state.brightness) / 255;

    ESP_LOGV(TAG, "Brightness adjusted color: R=%d, G=%d, B=%d\n", r, g, b);
    led_strip_clear(led_strip);

    for (uint8_t i = 0; i < 25; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }

    led_strip_refresh(led_strip);
}

void fill_led_strip_with_colorValues(colorValues_t colorValues) {
    fill_led_strip(colorValues.r, colorValues.g, colorValues.b);
}

void fill_led_strip_with_color(color_t color) {
    ESP_LOGV(TAG, "Color for filling LED strip is %d", color);
    fill_led_strip_with_colorValues(color_values[color]);
}

void toogle_led(bool state) {
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return;
    }
    ESP_LOGI(TAG, "Toggling LED state");

    current_led_state.state = state;
    if (state) {
        ESP_LOGD(TAG, "Turning on LED strip");
        fill_led_strip_with_colorValues(current_led_state.color);
    } else {
        ESP_LOGD(TAG, "Turning off LED strip");
        led_strip_clear(led_strip);
    }
}

current_led_state_t get_current_led_state(void) {
    return current_led_state;
}

void led_init(void) {
    ESP_LOGI(TAG, "project configured with addressable led strip!");
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
