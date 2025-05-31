#ifndef MAIN_BLEDEVICE_H_
#define MAIN_BLEDEVICE_H_

#include <inttypes.h>
#include "esp_log.h"
#include "esp_err.h"
#include "host/ble_gap.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define GATT_CUSTOM_SERVICE_UUID               "7f52a68c-45b2-4333-9cf3-cc377ef2ccdb"
#define CATT_CUSTOM_BUTTON_UUID           "edd6a5ba-9285-439b-a46d-dccaf3e82cf0"
#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

esp_err_t ble_device_init(void);
void ble_device_start(void);
void ble_device_notify(int16_t data);

#endif /* MAIN_BLEDEVICE_H_ */