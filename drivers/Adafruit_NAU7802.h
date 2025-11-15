/**************************************************************************/
/**
  @file     Adafruit_NAU7802.h

  Author: Timothy James Lim (METS Student)
  Credits to: Limor Fried (Adafruit Industries)

  This driver file is based on the work by Limor Fried for the Arduino
  ----> https://github.com/adafruit/Adafruit_NAU7802/tree/master

*/
/**************************************************************************/
#ifndef NAU7802_H
#define NAU7802_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

class NAU7802 {
public:
    // pass bus and pins so you can reuse for other I2C
    NAU7802(i2c_inst_t *bus, uint sda, uint scl, uint8_t addr = 0x2A);

    bool begin();              // init chip, check revision, set defaults
    bool available();          // data ready
    int32_t read();            // read 24-bit, sign-extended

    bool calibrate(uint8_t mode = 0);  // 0 = internal
    bool setRate(uint8_t rate);        // 0..7 like Adafruit
    bool setGain(uint8_t gain);        // 0..7
    bool setLDO(uint8_t ldo);          // 0..7, or external

private:
    bool reset();
    bool enable(bool on);

    bool writeReg(uint8_t reg, uint8_t val);
    bool readReg(uint8_t reg, uint8_t &val);

    i2c_inst_t *bus_;
    uint sda_;
    uint scl_;
    uint8_t addr_;
};

#endif