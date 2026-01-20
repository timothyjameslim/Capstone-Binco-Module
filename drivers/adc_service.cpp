#include "adc_service.h"

ADCService::ADCService(Adafruit_NAU7802 &adc)
        : adc_(adc) {}

bool ADCService::init(i2c_inst_t *i2c) {
    if (!adc_.begin(i2c)) {
        return false;
    }

    // Internal NAU7802 calibration
    if (!adc_.calibrate(NAU7802_CALMOD_INTERNAL)) {
        return false;
    }

    return true;
}

void ADCService::enable_iir(float cutoff_hz, float sample_hz) {
    const float omega = 2.0f * 3.1415926f * cutoff_hz;
    iir_.alpha = omega / (omega + sample_hz);
    iir_.y     = 0.0f;
    iir_.en    = true;
}

void ADCService::tare(float reference_grams) {
    // Average more samples for stable tare
    tare_offset_counts_ = read_filtered_counts_(64);
    tare_ref_grams_     = reference_grams;

    has_tare_ = true;
}

void ADCService::calibrate_with_weight(float known_grams) {
    if (!has_tare_) return;

    int32_t now = read_filtered_counts_(64);
    int32_t net = now - tare_offset_counts_;

    // known_grams must be heavier than tare reference
    if (net <= 0 || known_grams <= tare_ref_grams_) {
        return;
    }

    counts_per_gram_ =
            static_cast<float>(net) / (known_grams - tare_ref_grams_);

    has_cal_ = true;
}

float ADCService::read_grams() {
    int32_t raw = read_filtered_counts_(8);

    if (!has_tare_) {
        // fallback: uncalibrated, return counts-as-grams
        return static_cast<float>(raw);
    }

    int32_t net = raw - tare_offset_counts_;

    float grams =
            static_cast<float>(net) / counts_per_gram_ + tare_ref_grams_;

    return (grams < 0.0f) ? 0.0f : grams;
}

int32_t ADCService::read_filtered_counts_(int samples) {
    int64_t sum = 0;

    for (int i = 0; i < samples; ++i) {
        while (!adc_.available()) {
            tight_loop_contents();
        }

        float x = static_cast<float>(adc_.read());

        if (iir_.en) {
            iir_.y += iir_.alpha * (x - iir_.y);
            sum += static_cast<int32_t>(iir_.y);
        } else {
            sum += static_cast<int32_t>(x);
        }
    }

    return static_cast<int32_t>(sum / samples);
}