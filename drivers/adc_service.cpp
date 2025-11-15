#include "adc_service.h"
#include "Adafruit_NAU7802.h"   // your low-level driver

ADCService::ADCService(NAU7802 &adc)
        : adc_(adc) {}

bool ADCService::init() {
    if (!adc_.begin()) {
        return false;
    }
    // internal chip calibration
    adc_.calibrate(0);
    return true;
}

void ADCService::tare() {
    // average more for tare
    tare_offset_ = read_avg_(32);
    has_tare_ = true;
}

void ADCService::calibrate_with_weight(float known_grams) {
    // assume tare already done, user has placed known weight
    int32_t now = read_avg_(32);
    int32_t net = now - tare_offset_;
    if (net < 1) {
        // avoid zero / nonsense
        counts_per_gram_ = 1.0f;
    } else {
        counts_per_gram_ = static_cast<float>(net) / known_grams;
    }
    has_cal_ = true;
}

float ADCService::read_grams() {
    int32_t raw = read_avg_(4);           // fast-ish
    int32_t net = raw;

    if (has_tare_) {
        net -= tare_offset_;
    }
    if (net < 0) net = 0;

    if (has_cal_) {
        return static_cast<float>(net) / counts_per_gram_;
    } else {
        // not calibrated yet: return counts as grams-like
        return static_cast<float>(net);
    }
}

int32_t ADCService::read_avg_(int samples) {
    int64_t sum = 0;
    for (int i = 0; i < samples; ++i) {
        sum += adc_.read();
    }
    return static_cast<int32_t>(sum / samples);
}