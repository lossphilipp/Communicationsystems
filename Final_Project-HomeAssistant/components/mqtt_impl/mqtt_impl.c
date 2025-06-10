#include "mqtt_impl.h"

static esp_mqtt_client_handle_t gClient = NULL;
uint8_t current_subscriptions = 0;

static const char *TAG = "MQTT";

static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// ----- implementation -----

void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    gClient = event->client;
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld, client=%p", base, event_id, gClient); //", , );
    int msgId;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_init() {
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URL,
        .credentials.username = CONFIG_MQTT_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_BROKER_PASSWORD,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config);
    // The last argument may be used to pass data to the event handler, in this example mqtt_event_handler
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_subscribe(const char* topic) {
    if (gClient == NULL || topic == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized or topic is NULL");
        return;
    }

    if (current_subscriptions >= CONFIG_MQTT_MAX_SUBSCRIPTIONS) {
        ESP_LOGE(TAG, "Maximum number of MQTT subscriptions reached: %d", CONFIG_MQTT_MAX_SUBSCRIPTIONS);
        return;
    }
    current_subscriptions++;

    int msgId = esp_mqtt_client_subscribe(gClient, topic, 0);
    if (msgId < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return;
    }
    ESP_LOGI(TAG, "Subscribed to topic: %s, msg_id=%d", topic, msgId);
}

void mqtt_subscribe_callback(const char* topic, mqtt_message_callback_t callback) {
    mqtt_subscribe(topic);
    esp_mqtt_client_register_event(gClient, MQTT_EVENT_DATA, callback, NULL);
}

#if USE_DEFAULT_TOPIC
void mqtt_sendpayload(uint8_t* payload, uint16_t payloadLen) {
    if (gClient == NULL) {
        return;
    }
    int msgId = esp_mqtt_client_publish(gClient, CONFIG_MQTT_TOPIC, (char*)payload, payloadLen, 1, 0);
    ESP_LOGI(TAG, "sent publish successful\n msg_id: %d", msgId);
}
#else
void mqtt_sendpayload(const char* topic, uint8_t* payload, uint16_t payloadLen) {
    if (topic == NULL || gClient == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized or topic is NULL");
        return;
    }

    // Check if prefix is set and concatenate if needed
    const char* prefix = CONFIG_MQTT_TOPIC_PREFIX;
    char full_topic[256];
    if (prefix[0] != '\0') {
        // Ensure no double slash
        if (prefix[strlen(prefix) - 1] == '/' || topic[0] == '/') {
            snprintf(full_topic, sizeof(full_topic), "%s%s", prefix, topic);
        } else {
            snprintf(full_topic, sizeof(full_topic), "%s/%s", prefix, topic);
        }
        topic = full_topic;
    }

    int msgId = esp_mqtt_client_publish(gClient, topic, (char*)payload, payloadLen, 1, 0);
    ESP_LOGI(TAG, "sent publish successful\n topic: %s\n msg_id: %d", topic, msgId);
}
#endif