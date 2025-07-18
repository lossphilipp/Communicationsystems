#ifndef LED_H
#define LED_H

#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "sdkconfig.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_PERIOD CONFIG_BLINK_PERIOD

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

typedef struct {
    colorValues_t color;
    uint8_t brightness;
    bool state;
} current_led_state_t;

extern const colorValues_t color_values[];

void adjust_led_brightness(uint8_t brightness);
void fill_led_strip(uint8_t r, uint8_t g, uint8_t b);
void fill_led_strip_with_colorValues(colorValues_t colorValues);
void fill_led_strip_with_color(color_t color);
void toogle_led(bool state);
current_led_state_t get_current_led_state(void);
void led_init(void);

#endif // LED_H
