#include "mqtt_impl.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t gClient = NULL;
uint8_t current_subscriptions = 0;

static topic_callback_t topic_callbacks[CONFIG_MQTT_MAX_SUBSCRIPTIONS];
static uint8_t callback_count = 0;

static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static TaskHandle_t mqtt_notify_task = NULL;

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
    ESP_LOGD(TAG, "Event dispatched from event loop\nbase=%s, event_id=%ld, client=%p", base, event_id, gClient);
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (mqtt_notify_task) {
            xTaskNotifyGive(mqtt_notify_task);
        }
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
        ESP_LOGD(TAG, "GOT EVENT\n\tTOPIC: %.*s\n\tDATA: %.*s", event->topic_len, event->topic, event->data_len, event->data);
        
        // Handle topic-specific callbacks
        for (int i = 0; i < callback_count; i++) {
            if (strncmp(topic_callbacks[i].topic, event->topic, event->topic_len) == 0 && 
                strlen(topic_callbacks[i].topic) == event->topic_len) {
                if (topic_callbacks[i].callback) {
                    // Create null-terminated strings for the callback
                    char topic_str[128];
                    char data_str[256];
                    
                    snprintf(topic_str, sizeof(topic_str), "%.*s", event->topic_len, event->topic);
                    snprintf(data_str, sizeof(data_str), "%.*s", event->data_len, event->data);
                    
                    topic_callbacks[i].callback(topic_str, data_str);
                }
                break;
            }
        }
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

esp_err_t mqtt_init() {
    mqtt_notify_task = xTaskGetCurrentTaskHandle();
    const esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URL,
        .credentials.username = CONFIG_MQTT_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_BROKER_PASSWORD,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config);
    // The last argument may be used to pass data to the event handler, in this example mqtt_event_handler
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    // Wait for connection (timeout 10 seconds)
    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));
    if (notified == 0) {
        ESP_LOGE(TAG, "MQTT connection timeout");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT client initialized and connected successfully\n");
        return ESP_OK;
    }
}

void mqtt_get_full_topic(const char* topic, char* out_buf, size_t buf_size) {
    if (topic == NULL) {
        ESP_LOGE(TAG, "Topic is NULL");
        if (out_buf && buf_size > 0) {
            out_buf[0] = '\0';
        }
        return;
    }
    const char* prefix = CONFIG_MQTT_TOPIC_PREFIX;
    if (prefix[0] != '\0') {
        if (prefix[strlen(prefix) - 1] == '/' || topic[0] == '/') {
            snprintf(out_buf, buf_size, "%s%s", prefix, topic);
        } else {
            snprintf(out_buf, buf_size, "%s/%s", prefix, topic);
        }
    } else {
        snprintf(out_buf, buf_size, "%s", topic);
    }
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

    char full_topic[128];
    mqtt_get_full_topic(topic, full_topic, sizeof(full_topic));

    int msgId = esp_mqtt_client_subscribe(gClient, full_topic, 0);
    if (msgId < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", full_topic);
        return;
    }
    ESP_LOGI(TAG, "Subscribed to topic: %s, msg_id=%d", full_topic, msgId);
}

void mqtt_subscribe_callback(const char* topic, mqtt_message_callback_t callback) {
    if (topic == NULL || callback == NULL) {
        ESP_LOGE(TAG, "Topic or callback is NULL");
        return;
    }
    
    if (callback_count >= CONFIG_MQTT_MAX_SUBSCRIPTIONS) {
        ESP_LOGE(TAG, "Maximum number of topic callbacks reached: %d", CONFIG_MQTT_MAX_SUBSCRIPTIONS);
        return;
    }
    
    // Subscribe to the topic
    mqtt_subscribe(topic);
    
    // Store the callback mapping
    char full_topic[128];
    mqtt_get_full_topic(topic, full_topic, sizeof(full_topic));
    
    strncpy(topic_callbacks[callback_count].topic, full_topic, sizeof(topic_callbacks[callback_count].topic) - 1);
    topic_callbacks[callback_count].topic[sizeof(topic_callbacks[callback_count].topic) - 1] = '\0';
    topic_callbacks[callback_count].callback = callback;
    callback_count++;
    
    ESP_LOGI(TAG, "Registered callback for topic: %s", full_topic);
}

#if USE_DEFAULT_TOPIC
void mqtt_sendpayload(uint8_t* payload, uint16_t payloadLen) {
    if (gClient == NULL) {
        return;
    }
    int msgId = esp_mqtt_client_publish(gClient, CONFIG_MQTT_TOPIC, (char*)payload, payloadLen, 1, 0);
    ESP_LOGD(TAG, "Sent publish successful\n\payload: %.*s\n\tmsg_id: %d", payloadLen, payload, msgId);
}
#else
void mqtt_sendpayload(const char* topic, uint8_t* payload, uint16_t payloadLen) {
    if (topic == NULL || gClient == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized or topic is NULL");
        return;
    }

    char full_topic[128];
    mqtt_get_full_topic(topic, full_topic, sizeof(full_topic));

    int msgId = esp_mqtt_client_publish(gClient, full_topic, (char*)payload, payloadLen, 1, 0);
    ESP_LOGD(TAG, "Sent publish successful\n\ttopic: %s\n\tpayload: %.*s\n\tmsg_id: %d", full_topic, payloadLen, payload, msgId);
}
#endif