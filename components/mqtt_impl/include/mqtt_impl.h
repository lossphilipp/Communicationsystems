#ifndef MAIN_MQTT_H_
#define MAIN_MQTT_H_

#include <inttypes.h>
#include "esp_log.h"
#include "mqtt_client.h"

#ifndef CONFIG_MQTT_TOPIC_PREFIX
#define CONFIG_MQTT_TOPIC_PREFIX ""
#endif

void mqtt_init(void);

#if USE_DEFAULT_TOPIC
void mqtt_sendpayload(uint8_t* payload, uint16_t payloadLen);
#else
void mqtt_sendpayload(const char* topic, uint8_t* payload, uint16_t payloadLen);
#endif

#endif /* MAIN_MQTT_H_ */