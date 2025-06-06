#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <inttypes.h>
#include <math.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "sdkconfig.h"

#include "filter.h"
#include "firfilter.h"
#include "iirfilter.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define POTENTIOMETER_GPIO CONFIG_POTENTIOMETER_GPIO

typedef struct {
    float firValue_2;
    float firValue_10;
    float iirValue;
} potentiometer_filtered_t;

void potentiometer_init(void);
int32_t potentiometer_read_mV(void);
potentiometer_filtered_t potentiometer_read_filtered(void);
uint8_t potentiometer_read_percentage(void);
uint8_t potentiometer_read_uint8(void);

#endif /* POTENTIOMETER_H */