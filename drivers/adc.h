//
// Created by Timothy James Lim on 6/11/25.
//

#ifndef BINCO_ADC_H
#define BINCO_ADC_H

#pragma once
#include <string>
#include "pico/stdlib.h"
#include "../src/pinouts.h"

class adc {

public:
    void init();
    void clear();
    void tare();
    float read();

private:
    int32_t read_raw();
    int32_t offset =0;
};


#endif //BINCO_ADC_H
