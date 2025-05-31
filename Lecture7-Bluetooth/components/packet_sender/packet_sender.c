#include "packet_sender.h"

static const char* TAG = "PACKET_SENDER";

void packetsender_sendTCP(char* hostIP, uint16_t port, uint8_t* payload, uint16_t payloadLen) {
    int sock =  socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", hostIP, port);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(hostIP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Successfully connected");

    err = send(sock, payload, payloadLen, 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return;
    }

    ESP_LOGI(TAG, "Closing socket...");
    shutdown(sock, 0);
    close(sock);
}

void packetsender_sendUDP(char* hostIP, uint16_t port, uint8_t* payload, uint16_t payloadLen) {
    int sock =  socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket created, sending datagram to %s:%d", hostIP, port);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(hostIP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Successfully connected");

    err = send(sock, payload, payloadLen, 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return;
    }

    ESP_LOGI(TAG, "Closing socket...");
    shutdown(sock, 0);
    close(sock);
}