//
// Created by Timothy James Lim on 2/11/25.
//

#ifndef BINCO_LCD1602_H
#define BINCO_LCD1602_H

#pragma once
#include <string>
#include "pico/stdlib.h"
#include "../pinouts.h"

class LCD1602 {
public:
    LCD1602();
    void init();
    void clear();
    void setCursor(uint8_t col, uint8_t row);
    void print(const char *s);
    void print(const std::string &s);

private:
    void writeCmd(uint8_t cmd);
    void writeData(uint8_t data);

    // PCF8574 helpers
    void expanderWrite(uint8_t data);
    void pulseEnable(uint8_t data);
    void send(uint8_t value, bool rs);
    void write4bits(uint8_t value, bool rs);

    uint8_t addr_;
};

#endif //BINCO_LCD1602_H