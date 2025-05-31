#include "ble_device.h"

// ########## prototypes ##########
static void printAddr(const uint8_t* addr);
static int handleGAPEvent(struct ble_gap_event *event, void *arg);
static void enableAdvertising(void);

// ########## globals ##########
static const char *TAG = "BLE_DEVICE";
static const char *gDeviceName = "ESP32";
static const char *gManufacturerName = "lossphilipp";
static const char *gModelNum = "v0.1";

static uint8_t gAddrType;
static uint16_t gConnectionHandle;
static bool gNotifyState;
static uint16_t gButtonValueHandle;

// ########## implementation ##########

esp_err_t gattCharacteristicAccessDeviceInfo(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    esp_err_t rc = BLE_ATT_ERR_UNLIKELY;

    MODLOG_DFLT(INFO, "gattCharacteristicAccess: DeviceInfo\n connHandle=%d\n attrHandle=%d\n", connHandle, attrHandle);
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    if (uuid == GATT_MODEL_NUMBER_UUID) {
        if ((rc = os_mbuf_append(ctxt->om, gModelNum, strlen(gModelNum))) != ESP_OK) {
        	rc = BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    } else if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        if ((rc = os_mbuf_append(ctxt->om, gManufacturerName, strlen(gManufacturerName))) != ESP_OK) {
        	rc = BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    } else {
    	assert(0);
    }
    return rc;
}

esp_err_t gattCharacteristicAccessButton(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    esp_err_t rc = BLE_ATT_ERR_UNLIKELY;

    MODLOG_DFLT(INFO, "gattCharacteristicAccess: Button\n connHandle=%d\n attrHandle=%d\n", connHandle, attrHandle);
    uint8_t button_state = 1; // ToDo: Replace with actual button state retrieval logic 
    if ((rc = os_mbuf_append(ctxt->om, &button_state, sizeof(button_state))) != ESP_OK) {
        rc = BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return rc;
}

static const struct ble_gatt_svc_def gGATTServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(GATT_CUSTOM_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                .uuid = BLE_UUID128_DECLARE(CATT_CUSTOM_BUTTON_UUID),
                .access_cb = gattCharacteristicAccessButton,
                .val_handle = &gButtonValueHandle,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
            }, {
                0, /* No more characteristics in this service */
            },
        }
    },
    {
        /* Service: Device Information */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /* Characteristic: * Manufacturer name */
                .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                .access_cb = gattCharacteristicAccessDeviceInfo,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                /* Characteristic: Model number string */
                .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                .access_cb = gattCharacteristicAccessDeviceInfo,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                0, /* No more characteristics in this service */
            },
        }
    },
    {
        0, /* No more services */
    },
};

// ########## Implementation of BLE Device ##########

void printAddr(const uint8_t* addr) {
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

int handleGAPEvent(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            // A new connection was established or a connection attempt failed
            MODLOG_DFLT(INFO, "connection %s; status=%d\n", event->connect.status == 0 ? "established" : "failed", event->connect.status);
            if (event->connect.status != 0) {
                // Connection failed; resume advertising
                enableAdvertising();
            }
            gConnectionHandle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);
            gConnectionHandle = 0;
            // Connection terminated; resume advertising
            enableAdvertising();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            MODLOG_DFLT(INFO, "adv complete\n");
            enableAdvertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            MODLOG_DFLT(INFO, "subscribe event\n cur_notify=%d\n val_handle=%d\n", event->subscribe.cur_notify, gButtonValueHandle);
            if (event->subscribe.attr_handle == gButtonValueHandle) {
                gNotifyState = event->subscribe.cur_notify;
            } else if (event->subscribe.attr_handle != gButtonValueHandle) {
                gNotifyState = event->subscribe.cur_notify;
            }
            ESP_LOGI("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", gConnectionHandle);
            break;

        case BLE_GAP_EVENT_MTU:
            MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n", event->mtu.conn_handle, event->mtu.value);
            break;
    }
    return 0;
}

/* Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
void enableAdvertising() {
    struct ble_gap_adv_params advParams;
    struct ble_hs_adv_fields fields;
    int rc;

    /*  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));
    /* Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)gDeviceName;
    fields.name_len = strlen(gDeviceName);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    // Begin advertising
    memset(&advParams, 0, sizeof(advParams));
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(gAddrType, NULL, BLE_HS_FOREVER, &advParams, handleGAPEvent, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

void onSync() {
    int rc;

    rc = ble_hs_id_infer_auto(0, &gAddrType);
    assert(rc == 0);

    uint8_t address[6] = {0};
    rc = ble_hs_id_copy_addr(gAddrType, address, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    printAddr(address);
    MODLOG_DFLT(INFO, "\n");

    enableAdvertising();
}

void onReset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void bleHostTaskMain(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

esp_err_t initGATTServer() {
    ble_svc_gap_init();
    ble_svc_gatt_init();

    esp_err_t rc = ESP_OK;
    if ((rc = ble_gatts_count_cfg(gGATTServices)) != ESP_OK) {
        return rc;
    }
    if ((rc = ble_gatts_add_svcs(gGATTServices)) != ESP_OK) {
        return rc;
    }
    return ESP_OK;
}

esp_err_t ble_device_init() {
    esp_err_t rc = ESP_OK;
    // Initialize NimBLE
    nimble_port_init();
    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.reset_cb = onReset;

    if ((rc = initGATTServer()) != ESP_OK) {
        return rc;
    }
    assert(rc == 0);

    // Set the default device name; could also be done via sdkconfig
    if ((rc = ble_svc_gap_device_name_set(gDeviceName)) != ESP_OK) {
        return rc;
    }

    return ESP_OK;
}

void ble_device_start() {
    // Start the task
    nimble_port_freertos_init(bleHostTaskMain);
}

void ble_device_notify(int16_t data) {
    if (!gNotifyState) { // only if notification is on
        return;
    }
    MODLOG_DFLT(INFO, "Notify\n data=%d", data);

    struct os_mbuf* om = ble_hs_mbuf_from_flat(&data, sizeof(data));
    esp_err_t rc = ble_gattc_notify_custom(gConnectionHandle, gButtonValueHandle, om);
    assert(rc == 0);
}