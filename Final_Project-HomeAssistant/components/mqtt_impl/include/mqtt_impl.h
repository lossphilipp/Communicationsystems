#ifndef MAIN_MQTT_H_
#define MAIN_MQTT_H_

#include <inttypes.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef CONFIG_MQTT_TOPIC_PREFIX
#define CONFIG_MQTT_TOPIC_PREFIX ""
#endif

typedef void (*mqtt_message_callback_t)(const char* topic, const char* payload);

esp_err_t mqtt_init(void);
void mqtt_subscribe(const char* topic);
void mqtt_subscribe_callback(const char* topic, mqtt_message_callback_t callback);

#if USE_DEFAULT_TOPIC
void mqtt_sendpayload(uint8_t* payload, uint16_t payloadLen);
#else
void mqtt_sendpayload(const char* topic, uint8_t* payload, uint16_t payloadLen);
#endif

#endif /* MAIN_MQTT_H_ */