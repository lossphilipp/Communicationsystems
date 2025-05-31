#ifndef MAIN_PACKETSENDER_H_
#define MAIN_PACKETSENDER_H_

#include <inttypes.h>
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

void packetsender_sendTCP(char* hostIP, uint16_t port, uint8_t* payload, uint16_t payloadLen);
void packetsender_sendUDP(char* hostIP, uint16_t port, uint8_t* payload, uint16_t payloadLen);

#endif /* MAIN_PACKETSENDER_H_ */
