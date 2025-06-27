#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "led_strip.h"

#define WIFI_SSID      "FRITZ!Box Loss"
#define WIFI_PASS      "Buergle10a6850Dornbirn"
#define MQTT_BROKER    "mqtt://192.168.0178.212"
#define MQTT_USER      "mqtt_device_username"
#define MQTT_PASS      "mqtt_device_password"
#define TAG            "MQTT_RGB"

#define LED_PIN        2
#define RGB_PIN        16
#define RGB_LED_NUM    1

static esp_mqtt_client_handle_t client = NULL;
static bool rgb_on = false;
static uint8_t rgb_r = 255, rgb_g = 255, rgb_b = 255;

static led_strip_handle_t led_strip;

static void apply_rgb()
{
    if (rgb_on)
        led_strip_set_pixel(led_strip, 0, rgb_r, rgb_g, rgb_b);
    else
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);

    led_strip_refresh(led_strip);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        esp_mqtt_client_subscribe(client, "home/esp32/led", 0);
        esp_mqtt_client_subscribe(client, "home/esp32/rgb/set", 0);
        break;

    case MQTT_EVENT_DATA:
        if (strncmp(event->topic, "home/esp32/led", event->topic_len) == 0) {
            if (strncmp(event->data, "ON", event->data_len) == 0) {
                gpio_set_level(LED_PIN, 1);
                esp_mqtt_client_publish(client, "home/esp32/led/state", "ON", 0, 1, true);
            } else {
                gpio_set_level(LED_PIN, 0);
                esp_mqtt_client_publish(client, "home/esp32/led/state", "OFF", 0, 1, true);
            }
        } else if (strncmp(event->topic, "home/esp32/rgb/set", event->topic_len) == 0) {
            // crude JSON parser (expects {"state":"ON","color":{"r":255,"g":0,"b":0}})
            const char *json = event->data;
            if (strstr(json, "\"state\":\"OFF\"")) rgb_on = false;
            else if (strstr(json, "\"state\":\"ON\"")) rgb_on = true;

            char *r = strstr(json, "\"r\":");
            char *g = strstr(json, "\"g\":");
            char *b = strstr(json, "\"b\":");
            if (r) rgb_r = atoi(r + 4);
            if (g) rgb_g = atoi(g + 4);
            if (b) rgb_b = atoi(b + 4);

            apply_rgb();

            char buf[128];
            snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"color\":{\"r\":%d,\"g\":%d,\"b\":%d}}",
                     rgb_on ? "ON" : "OFF", rgb_r, rgb_g, rgb_b);
            esp_mqtt_client_publish(client, "home/esp32/rgb/state", buf, 0, 1, true);
        }
        break;
    default:
        break;
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_PIN,
        .max_leds = RGB_LED_NUM,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_clear(led_strip);

    nvs_flash_init();
    wifi_init_sta();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
