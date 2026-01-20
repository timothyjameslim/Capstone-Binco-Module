#ifndef BINCO_ADC_SERVICE_H
#define BINCO_ADC_SERVICE_H

#include "pico/stdlib.h"
#include "Adafruit_NAU7802.h"

/**
 * High-level ADC service for NAU7802 + load cell.
 *
 * Pipeline:
 *   raw ADC counts
 *     → IIR filter (counts domain)
 *     → tare subtraction (reference mass)
 *     → scale conversion
 *     → grams
 */
class ADCService {
public:
    explicit ADCService(Adafruit_NAU7802 &adc);

    // Initialize ADC + internal NAU7802 calibration
    bool init(i2c_inst_t *i2c);

    // Enable IIR low-pass filter (must be called BEFORE tare)
    // cutoff_hz  : filter cutoff frequency
    // sample_hz  : ADC sample rate (e.g. 10 for 10 SPS)
    void enable_iir(float cutoff_hz, float sample_hz);

    // Tare using a known reference mass already on the scale (e.g. 6.56 g)
    void tare(float reference_grams);

    // Optional second-point calibration using a heavier known mass
    void calibrate_with_weight(float known_grams);

    // Read weight in grams (blocking, filtered, calibrated)
    float read_grams();

private:
    // --- low-level ---
    Adafruit_NAU7802 &adc_;

    // --- IIR filter state ---
    struct {
        float y     = 0.0f;
        float alpha = 0.0f;
        bool  en    = false;
    } iir_;

    // --- calibration state ---
    int32_t tare_offset_counts_ = 0;   // filtered counts @ reference mass
    float   tare_ref_grams_     = 0.0f;

    float   counts_per_gram_    = 1.0f;

    bool    has_tare_           = false;
    bool    has_cal_            = false;

    // --- helpers ---
    int32_t read_filtered_counts_(int samples);
};

#endif // BINCO_ADC_SERVICE_H