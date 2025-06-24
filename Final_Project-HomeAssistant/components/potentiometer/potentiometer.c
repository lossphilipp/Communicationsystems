#include "potentiometer.h"

static const char *TAG = "POTENTIOMETER";

adc_oneshot_unit_handle_t adcHandle = NULL;
adc_cali_handle_t calHandle = NULL;

Filter* pFIRFilter_order2 = NULL;
Filter* pFIRFilter_order10 = NULL;
Filter* pIIRFilter = NULL;

void potentiometer_init() {
    // Use ADC1 in oneshot mode
    adc_oneshot_unit_init_cfg_t adcConfig = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adcConfig, &adcHandle));

    // ADC1 config; channel 2 maps on GPIO2
    adc_oneshot_chan_cfg_t channelConfig = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcHandle, ADC_CHANNEL_2, &channelConfig));

    ESP_LOGD(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t calConfig = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&calConfig, &calHandle));

    // Create Filters
    float b2[2] = { 0.5, 0.5 };
    pFIRFilter_order2 = firfilter_create(b2, 2);
    float b10[10] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 };
    pFIRFilter_order10 = firfilter_create(b10, 10);
    pIIRFilter = iirfilter_create(0.4142, 0.0, 0.2929, 0.2929, 0.0);

    ESP_LOGI(TAG, "Potentiometer configured\n"); 
}

int potentiometer_read_mV(){
    int rawValue, voltage_mV;
    adc_oneshot_read(adcHandle, ADC_CHANNEL_2, &rawValue);
    adc_cali_raw_to_voltage(calHandle, rawValue, &voltage_mV);

    ESP_LOGD(TAG, "Raw: %d, Voltage: %d mV", rawValue, voltage_mV);
    return voltage_mV;
}

potentiometer_filtered_t potentiometer_read_filtered(){
    potentiometer_filtered_t filter;
    int voltage_mV = potentiometer_read_mV();

    filter.firValue_2 = filter_filterValue(pFIRFilter_order2, voltage_mV);
    filter.firValue_10 = filter_filterValue(pFIRFilter_order10, voltage_mV);
    filter.iirValue = filter_filterValue(pIIRFilter, voltage_mV);

    ESP_LOGD(TAG, "Filtered values: FIR2: %.2f, FIR10: %.2f, IIR: %.2f", 
             filter.firValue_2, filter.firValue_10, filter.iirValue);

    return filter;
}

uint8_t potentiometer_read_percentage(){
    potentiometer_filtered_t filtered = potentiometer_read_filtered();

    #if (CONFIG_POTENTIOMETER_ADC_FILTER_FIR_2 == true)
        int16_t resist_ohm = (filtered.firValue_2 * 10000) / 2500;
    #elif (CONFIG_POTENTIOMETER_ADC_FILTER_FIR_10 == true)
        int32_t resist_ohm = (filtered.firValue_10 * 10000) / 2500;
    #elif (CONFIG_POTENTIOMETER_ADC_FILTER_IIR == true)
        int32_t resist_ohm = (filtered.iirValue * 10000) / 2500;
    #else
        int32_t resist_ohm = (potentiometer_read_mV() * 10000) / 2500;
    #endif

    uint8_t percentage = MIN((resist_ohm * 100) / 10000, 100);
    ESP_LOGD(TAG, "Resistance: %" PRId32 " ohm, Percentage: %d%%", resist_ohm, percentage);
    return percentage;
}

uint8_t potentiometer_read_uint8(){
    uint8_t percentage = potentiometer_read_percentage();
    uint8_t value = MIN(powf(percentage / 6.25, 2), 255); // scale to [0..16], then to [0..256]

    ESP_LOGD(TAG, "uint8 scaled value: %d", value);
    return value;
}