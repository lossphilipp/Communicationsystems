#ifndef MAIN_MQTT_H_
#define MAIN_MQTT_H_

#include <inttypes.h>
#include "esp_log.h"
#include "mqtt_client.h"

void mqtt_init(void);
void sendpacket_sendMQTT(uint8_t* payload, uint16_t payloadLen);

#endif /* MAIN_MQTT_H_ */