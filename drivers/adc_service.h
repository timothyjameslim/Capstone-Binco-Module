//
// Created by Timothy James Lim on 9/11/25.
//

#ifndef BINCO_ADC_SERVICE_H
#define BINCO_ADC_SERVICE_H

#include "pico/stdlib.h"

// forward-declare your driver
class NAU7802;

/**
 * High-level ADC wrapper for loadcell.
 * Handles init, tare, calibration, and per-loop reading.
 */
class ADCService {
public:
    // create with reference to an already-constructed NAU7802
    ADCService(NAU7802 &adc);

    // 1. single init
    bool init();

    // 3. calibration + tare
    // call once with empty scale -> stores baseline
    void tare();

    // call after placing a known weight (grams) on the scale.
    // this sets counts_per_gram.
    void calibrate_with_weight(float known_grams);

    // 2. single function for while-loop
    // reads ADC, averages, applies tare + calibration, returns grams
    float read_grams();

private:
    NAU7802 &adc_;
    int32_t  tare_offset_      = 0;
    float    counts_per_gram_  = 1.0f;  // avoid div by zero
    bool     has_tare_         = false;
    bool     has_cal_          = false;

    int32_t read_avg_(int samples);
};

#endif //BINCO_ADC_SERVICE_H
